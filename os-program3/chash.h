#include "clist.h"
#include "iostream"
#pragma once
#include<vector>

//a thread-safe (concurrent) hash table of integers, implemented as an array of linked lists.
class chash {
	/// The bucket list
	std::vector<clist> buckets;
	size_t size;
	size_t getKey(int val) const {
		return val % size;
	}
public:
	chash(unsigned _buckets) : buckets(std::vector<clist>(_buckets, clist(0))), size(_buckets) {}
	/// insert *key* into the appropriate linked list if it doesn't already
	/// exist; return true if the key was added successfully.
	bool insert(int key) {
		size_t placement = getKey(key);
		bool retval = buckets[placement].insert(key);
		return retval;
	}
	/// remove *key* from the appropriate list if it was present; return true
	/// if the key was removed successfully.
	bool remove(int key) {
		size_t placement = getKey(key);
		return buckets[placement].remove(key);
	}
	/// return true if *key* is present in the appropriate list, false
	/// otherwise
	bool lookup(int key) const {
		size_t placement = getKey(key);
		return buckets[placement].lookup(key);
	}

	//The following are not tested by the given tester but are required for grading
	//No locks are required for these.

	//This refers to the number of buckets not the total number of elements.
	size_t getSize() const {
		return size;
	}
	size_t getBucketSize(size_t bucket) const {
		if (bucket >= size) {
			return 0;
		}
		return buckets[bucket].getSize();
	}

	int getElement(size_t bucket, size_t idx) const {
		if (bucket >= size) {
			return -1;
		}
		return buckets[bucket].getElement(idx);
	}

	int getElement(size_t idx) const
	{
		//Do not edit this
		return 0;
	}
};
