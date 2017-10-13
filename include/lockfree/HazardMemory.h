#ifndef REDSCRIPT_LOCKFREE_HAZARDMEMORY_H
#define REDSCRIPT_LOCKFREE_HAZARDMEMORY_H

#include <atomic>
#include <vector>
#include <cstdint>
#include <utility>
#include <stdexcept>
#include <unordered_set>

#include "engine/Thread.h"
#include "engine/Memory.h"

#include "utils/Immovable.h"
#include "utils/NonCopyable.h"

/** Hazard Memory implementation
 *  based on paper "Efficient and Reliable Lock-Free Memory Reclamation Based on Reference Counting", Gidenstam, et al. - 2005
 */

namespace RedScript::LockFree
{
struct HazardNode : public Utils::Immovable, public Utils::NonCopyable
{
    std::atomic_int ref;
    std::atomic_bool del;
    std::atomic_bool trace;

public:
    HazardNode() : ref(0), del(false), trace(false) {}

};

struct HazardLink : public Utils::Immovable, public Utils::NonCopyable
{
    /* this field is just a pointer to `Node` */
    std::atomic<HazardNode *> data;

public:
    HazardLink() : HazardLink(nullptr) {}
    HazardLink(HazardNode *node) : data(node) {}

public:
    bool operator==(const HazardLink &other) const { return data.load() == other.data.load(); }
    bool operator!=(const HazardLink &other) const { return data.load() != other.data.load(); }

public:
    void setUnsafe(const HazardLink &value)
    {
        /* set the new link */
        HazardNode *next = value.data.load();
        HazardNode *prev = data.exchange(next);

        /* reference the new link */
        if (next)
        {
            next->ref++;
            next->trace = false;
        }

        /* then unref the old link */
        if (prev)
            prev->ref--;
    }

public:
    bool directCAS(const HazardLink &expect, const HazardLink &value)
    {
        /* load the current value */
        HazardNode *next = value.data.load();
        HazardNode *prev = expect.data.load();

        /* CAS the link directly, without modifying reference counter */
        return data.compare_exchange_strong(prev, next);
    }

public:
    bool referenceCAS(const HazardLink &expect, const HazardLink &value)
    {
        /* load the current value */
        HazardNode *next = value.data.load();
        HazardNode *prev = expect.data.load();

        /* CAS the link */
        if (!data.compare_exchange_strong(prev, next))
            return false;

        /* reference the new link */
        if (next)
        {
            next->ref++;
            next->trace = false;
        }

        /* then unref the old link */
        if (prev)
            prev->ref--;

        return true;
    }
};

struct HazardLimits
{
    static constexpr size_t MAX_LINKS   = 2;
    static constexpr size_t MAX_DELETES = 2;
    static constexpr size_t MAX_HAZARDS = 6;
    static constexpr size_t MAX_THREADS = Engine::Thread::MAX_THREADS;
};

template <typename Node = HazardNode, typename Link = HazardLink, typename Limits = HazardLimits>
struct HazardMemory
{
    static constexpr size_t UPDATE_THRESHOLD  = Limits::MAX_THREADS * (Limits::MAX_LINKS + Limits::MAX_DELETES + Limits::MAX_HAZARDS + 1);
    static constexpr size_t RECLAIM_THRESHOLD = Limits::MAX_HAZARDS * 2;

private:
    typedef std::atomic<HazardNode *> AtomicPNode;
    typedef std::unordered_set<HazardNode *> PNodeSet;

private:
    struct DeadNode
    {
        DeadNode         *next;
        AtomicPNode       node;
        std::atomic_int   claim;
        std::atomic_bool  finshed;

    public:
        DeadNode() : next(nullptr), node(nullptr), claim(0), finshed(false) {}

    };

/*** Per-thread Hazard Free-List ***/

private:
    struct ThreadData
    {
        int         refs[Limits::MAX_HAZARDS] = {0};
        AtomicPNode nodes[Limits::MAX_HAZARDS] = {};

    public:
        DeadNode *deadHead = nullptr;
        size_t    deadCount = 0;

    public:
        DeadNode deadNodes[UPDATE_THRESHOLD];
        PNodeSet deadHazards;

    public:
        std::vector<int>        hazardFreeList;
        std::vector<DeadNode *> deadNodeFreeList;

    public:
        HazardMemory *_self;

    public:
        ~ThreadData()
        {
            /* release all dead nodes */
            for (DeadNode *node = deadHead; node; node = node->next)
                delete node->node.load();
        }

    public:
        ThreadData(HazardMemory *self)
        {
            /* preserve space to prevent frequent `malloc` */
            hazardFreeList.reserve(Limits::MAX_HAZARDS);
            deadNodeFreeList.reserve(UPDATE_THRESHOLD);

            /* initialize hazard free list */
            for (int i = Limits::MAX_HAZARDS - 1; i >= 0; i--)
                hazardFreeList.push_back(i);

            /* initialize dead node free list */
            for (ssize_t i = UPDATE_THRESHOLD - 1; i >= 0; i--)
                deadNodeFreeList.push_back(&(deadNodes[i]));

            /* register to thread data list */
            _self = self;
            _self->_threadData[_self->_threadCount++] = this;
        }
    };

private:
    ThreadData *_threadData[Limits::MAX_THREADS];
    std::atomic_size_t _threadCount;
    Engine::Thread::Local<ThreadData *> _tls;

public:
    virtual ~HazardMemory()
    {
        /* clear all thread data */
        for (ThreadData *&tls : _threadData)
            delete tls;
    }

public:
    explicit HazardMemory() : _tls([=]{ return new (ThreadData *){ new ThreadData(this) }; })
    {
        _threadCount.store(0);
        memset(_threadData, 0, sizeof(_threadData));
    }

/*** Memory Management Functions ***/

public:
    void free(Node *node)
    {
        /* thread local data */
        ThreadData *tls = _tls.get();

        /* mark as deleted */
        node->del = true;
        node->trace = false;

        /* check for available dead node list */
        if (tls->deadNodeFreeList.empty())
            throw std::runtime_error("Insufficient dead node space");

        /* get an empty dead node */
        DeadNode *dhead = tls->deadNodeFreeList.back();
        tls->deadNodeFreeList.pop_back();

        /* initialize dead node, then insert into the dead node list */
        dhead->node = node;
        dhead->finshed = false;
        dhead->next = tls->deadHead;

        /* update dead node count and list */
        tls->deadHead = dhead;
        tls->deadCount++;

        /* reclaim dead nodes as much as possible */
        while (true)
        {
            /* update local nodes so links referencing deleted nodes are replaced with live nodes */
            if (tls->deadCount == UPDATE_THRESHOLD)
                for (DeadNode *p = tls->deadHead; p; p = p->next)
                    nodeUpdate(reinterpret_cast<Node *>(p->node.load()));

            /* reclaim if there are too much dead node */
            if (tls->deadCount >= RECLAIM_THRESHOLD)
            {
                /* set trace to make sure ref == 0 is consistent across hazard check below */
                for (DeadNode *p = tls->deadHead; p; p = p->next)
                {
                    HazardNode *pnode = p->node.load();
                    if (!pnode->ref.load())
                    {
                        pnode->trace = true;
                        if (pnode->ref.load())
                            pnode->trace = false;
                    }
                }

                /* flag all del nodes that have a hazard so they are not reclaimed */
                for (size_t i = 0; i < _threadCount.load(); i++)
                {
                    ThreadData *data = _threadData[i];
                    for (AtomicPNode &hazard : data->nodes)
                    {
                        HazardNode *pnode = hazard.load();
                        if (pnode) tls->deadHazards.insert(pnode);
                    }
                }

                size_t newDeadCount = 0;
                DeadNode *newDeadHead = nullptr;

                /* reclaim nodes and build new list of del nodes that could not be reclaimed */
                while (tls->deadHead)
                {
                    /* get the hazard node */
                    DeadNode *dnode = tls->deadHead;
                    HazardNode *pnode = dnode->node.load();

                    /* move the dead head to next node */
                    tls->deadHead = dnode->next;

                    /* check for hazard reference */
                    if (!pnode->ref.load() &&
                         pnode->trace.load() &&
                        !tls->deadHazards.count(pnode))
                    {
                        /* no reference found, reset the node pointer */
                        dnode->node = nullptr;

                        /* and if not claiming by other thread, claim it */
                        if (!dnode->claim.load())
                        {
                            nodeTerminate(reinterpret_cast<Node *>(pnode), false);
                            tls->deadNodeFreeList.push_back(dnode);
                            delete pnode;
                            continue;
                        }

                        /* terminate the node concurrently, and mark as claimed */
                        nodeTerminate(reinterpret_cast<Node *>(pnode), true);
                        dnode->finshed = true;
                        dnode->node = pnode;
                    }

                    /* update the dead-node list head and count */
                    dnode->next = newDeadHead;
                    newDeadHead = dnode;
                    newDeadCount++;
                }

                /* update dead node list */
                tls->deadHead = newDeadHead;
                tls->deadCount = newDeadCount;
                tls->deadHazards.clear();
            }

            /* check for dead node count */
            if (tls->deadCount != UPDATE_THRESHOLD)
                break;

            /* update links in all threads */
            for (size_t i = 0; i < _threadCount.load(); i++)
            {
                ThreadData *data = _threadData[i];
                for (DeadNode &deadNode : data->deadNodes)
                {
                    bool done = deadNode.finshed.load();
                    HazardNode *pnode = deadNode.node.load();

                    /* not claimed, and has nodes */
                    if (!done && pnode)
                    {
                        /* enter claim counter */
                        deadNode.claim++;

                        /* double-check to ensure the node was not claimed by another thread */
                        if (pnode == deadNode.node.load())
                            nodeUpdate(reinterpret_cast<Node *>(pnode));

                        /* leave claim counter */
                        deadNode.claim--;
                    }
                }
            }
        }
    }

public:
    template <typename ... Args>
    Node *alloc(Args &&... args)
    {
        /* instaniate new node, then add a reference to it */
        return retain(new Node(std::forward<Args>(args) ...));
    }

public:
    Node *retain(Node *node)
    {
        /* thread local data */
        ThreadData *tls = _tls.get();

        /* lookup nodes in hazard list */
        for (int i = 0; i < Limits::MAX_HAZARDS; i++)
        {
            if (tls->nodes[i] == node)
            {
                tls->refs[i]++;
                return node;
            }
        }

        /* check for available hazard list */
        if (tls->hazardFreeList.empty())
            throw std::runtime_error("Insufficient hazard pointer space");

        /* get an empty cell */
        int index = tls->hazardFreeList.back();
        tls->hazardFreeList.pop_back();

        /* reference into new hazard */
        tls->refs[index]++;
        tls->nodes[index] = node;
        return node;
    }

public:
    Node *retain(Link &link)
    {
        /* thread local data */
        ThreadData *tls = _tls.get();

        /* check for available hazard list */
        if (tls->hazardFreeList.empty())
            throw std::runtime_error("Insufficient hazard pointer space");

        /* get an empty cell */
        int found = -1;
        int index = tls->hazardFreeList.back();
        Node *node = nullptr;

        /* reference hazard pointer until success */
        do
        {
            node = link.ptr();
            tls->nodes[index] = node;
        } while (link.ptr() != node);

        /* only add hazard if pointer is valid */
        if (!node)
            return nullptr;

        /* check if hazard is already referenced by this thread */
        for (int i = 0; i < Limits::MAX_HAZARDS; i++)
        {
            if (i != index && tls->nodes[i] == node)
            {
                found = i;
                break;
            }
        }

        /* already referenced, use the existing one */
        if (found >= 0)
        {
            tls->refs[found]++;
            tls->nodes[index] = nullptr;
        }

        /* otherwise, use new hazard */
        else
        {
            tls->refs[index]++;
            tls->hazardFreeList.pop_back();
        }

        return node;
    }

public:
    void release(Node *node)
    {
        /* thread local data */
        ThreadData *tls = _tls.get();

        /* lookup nodes in hazard list */
        for (int i = 0; i < Limits::MAX_HAZARDS; i++)
        {
            if (tls->nodes[i] == node)
            {
                /* check for reference count */
                if (!(tls->refs[i]))
                    throw std::runtime_error("Node already released");

                /* decrease reference counter */
                if (!(--tls->refs[i]))
                {
                    tls->nodes[i] = nullptr;
                    tls->hazardFreeList.push_back(i);
                }

                return;
            }
        }

        /* hazard node not found, should not be possible */
        throw std::runtime_error("Hazard node not exists");
    }

protected:
    virtual void nodeUpdate(Node *self) = 0;
    virtual void nodeTerminate(Node *self, bool isConcurrent) = 0;

};
}

#endif /* REDSCRIPT_LOCKFREE_HAZARDMEMORY_H */
