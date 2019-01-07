#pragma once
#include <pthread.h>
#include <vector>

//a thread-safe (concurrent) AVL tree of integers, implemented as a set of Node objects.
//In addition, the API now allows for multiple insertions on each operation.
//Less than goes to the left. Greater than goes to the right.
class tree {
public:
	struct Node {
		public:
			int value;
			int height;
			Node* left;
			Node* right;
	};

	Node* constructNode(int val) {
		Node* obj = new Node;
		obj -> value = val;
		obj -> height = 1;
		obj -> left = NULL;
		obj -> right = NULL;
		return obj;
	}

	inline int getHeight(Node* root) {
		if (root == NULL) {
			return 0;
		}
		return root -> height;
	}

	inline int getSubTreeDifference(Node* root) {
		if (root == NULL) {
			return 0;
		}
		return getHeight(root -> left) - getHeight(root -> right);
	}

	inline int max(int a, int b) {
		if (a > b) {
			return a;
		}
		return b;
	}

	Node* rightRotate(Node *&root) {
		Node* pivot = root -> left;
		root -> left = pivot -> right;
		pivot -> right = root;

		root -> height = max(getHeight(root -> left), getHeight(root -> right)) + 1;
		pivot -> height = max(getHeight(pivot -> left), getHeight(pivot -> right)) + 1;

		return pivot;
	}

	Node* leftRotate(Node* root) {
		Node* pivot = root -> right;
		root -> right = pivot -> left;
		pivot -> left = root;

		root -> height = max(getHeight(root -> left), getHeight(root -> right)) + 1;
		pivot -> height = max(getHeight(pivot -> left), getHeight(pivot -> right)) + 1;

		return pivot;
	}

	Node* balance(Node* root) {
		int rootDifference = getSubTreeDifference(root);
		if (rootDifference > 1) {
			if (getSubTreeDifference(root -> left) < 0) {
				root -> left = leftRotate(root -> left);
			}
			root = rightRotate(root);
		}
		else if (rootDifference < -1) {
			if (getSubTreeDifference(root -> right) > 0) {
				root -> right = rightRotate(root -> right);
			}
			root = leftRotate(root);
		}
		return root;
	}

	Node* findMax(Node* root) {
	  if (root -> right == NULL) {
	    return root;
	  }
	  return findMax(root -> right);
	}

	Node* destroy(Node* root) {
		if (root == NULL) {
			return root;
		}
		root -> left = destroy(root -> left);
		root -> right = destroy(root -> right);
		delete root;
		return NULL;
	}

	Node* head;
	mutable pthread_rwlock_t rwlock;
public:
	tree(int) {
		this -> head = NULL;
		pthread_rwlock_init(&this -> rwlock, NULL);
	}
	~tree() {
		destroy(head);
		pthread_rwlock_destroy(&this -> rwlock);
	}

	//insert /num/ values from /data/ array into the tree, and return the
	//success/failure of each insert in /results/ array.
	void insert(int* data, bool* results, int num) {
		pthread_rwlock_wrlock(&this -> rwlock);
		for (int i = 0; i < num; i++) {
			head = insert(head, data[i], results[i]);
		}
		pthread_rwlock_unlock(&this -> rwlock);
	}

	Node* insert(Node* root, int &data, bool &inserted) {
		if (root == NULL) {
			root = constructNode(data);
			inserted = true;
			return root;
		}
		else if (data == root -> value) {
			inserted = false;
			return root;
		}
		else if (data < root -> value) {
			root -> left = insert(root -> left, data, inserted);
		}
		else {
			root -> right = insert(root -> right, data, inserted);
		}
		root -> height = max(getHeight(root -> left), getHeight(root -> right)) + 1;
		if (inserted) {
			return balance(root);
		}
		return root;
	}

	/// remove *data* from the list if it was present; return true if the data
	/// was removed successfully.
	void remove(int* data, bool* results, int num) {
		pthread_rwlock_wrlock(&this -> rwlock);
		for (int i = 0; i < num; i++) {
			head = remove(head, data[i], results[i]);
		}
		pthread_rwlock_unlock(&this -> rwlock);
	}

	Node* remove(Node* root, int &data, bool &removed) {
		if (root == NULL) {
			removed = false;
			return root;
		}
		else if (data < root -> value) {
			root -> left = remove(root -> left, data, removed);
		}
		else if (data > root -> value) {
			root -> right = remove(root -> right, data, removed);
		}
		else if (root -> right == NULL && root -> left == NULL) {
			delete root;
			root = NULL;
			removed = true;
		}
		else if (root -> left == NULL) {
			Node* temp = root;
			root = root -> right;
			delete temp;
			removed = true;
		}
		else if (root -> right == NULL) {
			Node* temp = root;
			root = root -> left;
			delete temp;
			removed = true;
		}
		else {
			Node* temp = findMax(root -> left);
			root -> value = temp -> value;
			root -> left = remove(root -> left, temp -> value, removed);
		}
		if (root == NULL) {
			return root;
		}
		root -> height = max(getHeight(root -> left), getHeight(root -> right)) + 1;
		if (removed) {
			return balance(root);
		}
		return root;
	}
	/// return true if *data* is present in the list, false otherwise
	void lookup(int* data, bool* results, int num) const {
		pthread_rwlock_rdlock(&this -> rwlock);
		for (int i = 0; i < num; i++) {
			results[i] = lookup(data[i]);
		}
		pthread_rwlock_unlock(&this -> rwlock);
	}

	bool lookup(int& data) const {
		Node* runner = head;
		while (runner != NULL) {
			if (data == runner -> value) {
				return true;
			}
			else if (data < runner -> value) {
				runner = runner -> left;
			}
			else {
				runner = runner -> right;
			}
		}
		return false;
	}

	//The following are not tested by the given tester but are required for grading
	//No locks are required for these.

	//Total number of elements in the tree
	size_t getSize() const {
		return getSize(head);
	}

	size_t getSize(Node* root) const {
		if (root == NULL) {
			return 0;
		}
		return 1 + getSize(root -> left) + getSize(root -> right);
	}

	int getElement(size_t idx) const {
		return getElement(idx, head);
	}

	size_t getBucketSize() const
	{
		//Do not edit this
		return 0;
	}
	int getElement(size_t bucket, size_t idx) const
	{
		//Do not edit this
		return 0;
	}


private:
	//Returns the idx'th element from the tree assuming an inorder traversal.
	int getElement(size_t idx, Node* at) const
	{
		std::vector<int> elements;
		getElement(idx, at, elements);
		if (idx >= elements.size()) {
			return -1;
		}
		return elements[idx];
	}
	void getElement(const size_t idx, Node* root, std::vector<int> &elements) const {
		if (root == NULL) {
			return;
		}
		getElement(idx, root -> left, elements);
		elements.push_back(root -> value);
		getElement(idx, root -> right, elements);
	}
};
