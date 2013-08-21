#ifndef cache_h
#define cache_h

/////////////////////////////////////////////////////////////////////////////
//
// A template for a cache - basically a hash_map that grows to a fixed
// size, after which oldest entries get pushed off to make room for new
// objects.

#include <map>
#include <list>

template <class Key, class Value>

class Cache {

	size_t cacheSize;

	std::map<Key, Value> hash;
	std::list<Key> queue;

public:

	typedef std::pair<const Key, Value> value_type;

	Cache(size_t cacheSizeArg=100) : cacheSize(cacheSizeArg) {}
	~Cache() {}

	void clear() { hash.clear(); queue.clear(); }

	void insert(const value_type &v)
	{
		if (hash.count(v.first) == 0)
		{
			if (queue.size() >= cacheSize)
			{
				hash.erase(*queue.begin());
				queue.erase(queue.begin());
			}

			queue.push_back(v.first);
		}
		else
		{
			hash.erase(v.first);
		}
		hash.insert(v);
	}


	size_t count(const Key &k) const { return hash.count(k); }
	const value_type *find(const Key &k) const
	{
		return &*hash.find(k);
	}

	value_type *find(const Key &k)
	{
		return &*hash.find(k);
	}
};

#endif
