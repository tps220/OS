#pragma once
#include <pthread.h>
#include <set>

//a thread-safe (concurrent) hash table of integers, implemented as an array of linked lists.
//In this implementation, each list should have a "sentinel" node that
//contains the lock, so we can't just reuse the clist implementation.
//In addition, the API now allows for multiple keys on each operation.
class shash2 {
	struct Node {
		const int value;
		Node* next;
		Node(int val): value(val), next(NULL) {}
	};

	struct SentinelNode {
		mutable pthread_rwlock_t rwlock;
		mutable Node* next;
		SentinelNode() {
			this -> next = NULL;
			pthread_rwlock_init(&this -> rwlock, NULL);
		}
	};

	void destroyList(const SentinelNode* root) {
		Node *runner = root -> next;
		while (runner != NULL) {
			Node* temp = runner -> next;
			delete runner;
			runner = temp;
		}
		pthread_rwlock_destroy(&root -> rwlock);
		delete root;
	}

	inline size_t getKey(int key) const {
		return key % size;
	}

	//data fields
	const SentinelNode** buckets;
	const size_t size;
public:
	//constructor
	shash2(unsigned _buckets) : buckets(new const SentinelNode*[_buckets]), size(_buckets) {
		for (int i = 0; i < _buckets; i++) {
			buckets[i] = new SentinelNode();
		}
	}

	~shash2() {
		for (int i = 0; i < size; i++) {
			destroyList(buckets[i]);
		}
		delete [] buckets;
	}

	//insert /num/ values from /keys/ array into the hash, and return the
	//success/failure of each insert in /results/ array.
	void insert(int* keys, bool* results, int num) {
		//create an ordered set of the buckets to be operated on
		std::set<int> map;
		for (int i = 0; i < num; i++) {
			map.insert(getKey(keys[i]));
		}

		//in ascending order of bucket indexes, attempt to lock all the critical
		//sections to maintain atomicity
		for (auto it = map.begin(); it != map.end(); it++) {
			const SentinelNode* root = this -> buckets[*it];
			pthread_rwlock_wrlock(&root -> rwlock);
		}

		//sequentially insert the keys
		for (int i = 0; i < num; i++) {
			int key = keys[i];
			const SentinelNode* root = this -> buckets[getKey(key)];
			//if the linked list is empty, create a new node and return true
			if (root -> next == NULL) {
				root -> next = new Node(key);
				results[i] = true;
				continue;
			}
			//if the head of list has value of the key, return false
			else if (root -> next -> value == key) {
				results[i] = false;
				continue;
			}
			//if the head of list's value is greater than the key, update head of linked list and return true
			else if (root -> next -> value > key) {
				Node* obj = new Node(key);
				obj -> next = root -> next;
				root -> next = obj;
				results[i] = true;
				continue;
			}

			//traverse the list until we reach the end, or have reached a value >= key
			Node* runner = root -> next;
			while (runner -> next != NULL && runner -> next -> value < key) {
				runner = runner -> next;
			}

			//if the key was found in the list during traversal,
			//return false and continue
			if (runner -> next && runner -> next -> value == key) {
				results[i] = false;
				continue;
			}
			Node* obj = new Node(key);
			obj -> next = runner -> next;
			runner -> next = obj;
			results[i] = true;
		}

		//in ascending order of bucket indexes, unlock all the critical sections.
		//since we've already done all of our work, we maintain atomicity throughout
		//this expensive loop
		for (auto it = map.begin(); it != map.end(); it++) {
			const SentinelNode* root = this -> buckets[*it];
			pthread_rwlock_unlock(&root -> rwlock);
		}
	}

	//delete /num/ values from /keys/ array into the hash, and return the
	//success/failure of each deletion in /results/ array
	void remove(int* keys, bool* results, int num) {
		//create an ordered set of the buckets to be operated on
		std::set<int> map;
		for (int i = 0; i < num; i++) {
			map.insert(getKey(keys[i]));
		}

		//in ascending order of bucket indexes, attempt to lock all the critical
		//sections to maintain atomicity
		for (auto it = map.begin(); it != map.end(); it++) {
			const SentinelNode* root = this -> buckets[*it];
			pthread_rwlock_wrlock(&root -> rwlock);
		}

		//sequentially delete the keys
		for (int i = 0; i < num; i++) {
			int key = keys[i];
			const SentinelNode* root = this -> buckets[getKey(key)];
			//if the linked list is empty, return false
			if (root -> next == NULL) {
				results[i] = false;
				continue;
			}
			//if the value is at the head of list, update the head value
			else if (root -> next -> value == key) {
				Node* obj = root -> next;
				root -> next = root -> next -> next;
				delete obj;
				results[i] = true;
				continue;
			}

			//traverse the list until we reach the end, or have reached a value >= key
			Node* runner = root -> next;
			while (runner -> next != NULL && runner -> next -> value < key) {
				runner = runner -> next;
			}
			//if the target value was found, remove it from the list and return true
			if (runner -> next && runner -> next -> value == key) {
				Node* obj = runner -> next;
				runner -> next = runner -> next -> next;
				delete obj;
				results[i] = true;
				continue;
			}
			//otherwise the target value was never found, return false
			results[i] = false;
		}

		//in ascending order of bucket indexes, unlock all the critical sections.
		//since we've already done all of our work, we maintain atomicity throughout
		//this expensive loop
		for (auto it = map.begin(); it != map.end(); it++) {
			const SentinelNode* root = this -> buckets[*it];
			pthread_rwlock_unlock(&root -> rwlock);
		}
	}

	//find /num/ values from /keys/ array into the hash, and return the
	//success/failure of each find in /results/ array
	void lookup(int* keys, bool* results, int num) const {
		//create an ordered set of the buckets to be operated on
		std::set<int> map;
		for (int i = 0; i < num; i++) {
			map.insert(getKey(keys[i]));
		}

		//in ascending order of bucket indexes, attempt to lock all the critical
		//sections to maintain atomicity
		for (auto it = map.begin(); it != map.end(); it++) {
			const SentinelNode* root = this -> buckets[*it];
			pthread_rwlock_rdlock(&root -> rwlock);
		}

		//sequentially find the keys
		for (int i = 0; i < num; i++) {
			int key = keys[i];
			Node* runner = this -> buckets[getKey(key)] -> next;
			while (runner != NULL && runner -> value < key) {
				runner = runner -> next;
			}
			if (runner && runner -> value == key) { results[i] = true; }
			else { results[i] = false; }
		}

		//in ascending order of bucket indexes, unlock all the critical sections.
		//since we've already done all of our work, we maintain atomicity throughout
		//this expensive loop
		for (auto it = map.begin(); it != map.end(); it++) {
			const SentinelNode* root = this -> buckets[*it];
			pthread_rwlock_unlock(&root -> rwlock);
		}
	}

	//The following are not tested by the given tester but are required for grading
	//No locks are required for these.

	//This refers to the number of buckets not the total number of elements.
	size_t getSize() const
	{
		return this -> size;
	}

	//This refers to the number of elements in a bucket, not the sentinel node.
	size_t getBucketSize(size_t bucket) const
	{
		//traverse the linked list and update the count
		if (bucket >= size) {
			return 0;
		}
		Node* runner = this -> buckets[bucket] -> next;
		size_t count = 0;
		while (runner != NULL) {
			count++;
			runner = runner -> next;
		}
		return count;
	}
	int getElement(size_t bucket, size_t idx) const
	{
		//traverse the linked list until we reach the index or the value of runner is NULL
		if (bucket >= size) {
			return -1;
		}
		Node* runner = this -> buckets[bucket] -> next;
		for (int i = 0; i < idx; i++) {
			if (runner == NULL) {
				return -1;
			}
			runner = runner -> next;
		}
		if (runner != NULL) {
			return runner -> value;
		}
		return -1;
	}


	int getElement(size_t idx) const
	{
		//Do not edit this
		return 0;
	}
};
