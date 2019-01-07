#include "tree.h"
#include <vector>
#include <set>
#include <unordered_map>
#include <iostream>
#include <algorithm>
#include <stdlib.h>
using namespace std;

#define SIZE 5120
#define BUCKETS 16

int main(int argc, char** argv) {
  set<int> map;
  srand(time(NULL));
  for (int i = 0; i < SIZE; i++) {
    map.insert(rand() % (SIZE * 10));
  }
  tree tree(BUCKETS);

  //create insertions
  vector<int> elements(map.begin(), map.end());
  vector< vector<int> > keys;
  for (int i = 0; i < SIZE / 64; i++) {
    keys.push_back(vector<int>(elements.begin() + i * 32, elements.begin() + (i + 1) * 32));
  }

  //create array of random elements
  for (int i = 0; i < keys.size(); i++) {
    int idx = rand() % keys.size();
    vector<int> temp = keys[i];
    keys[i] = keys[idx];
    keys[idx] = temp;
    for (int j = 0; j < keys[i].size(); j++) {
      idx = rand() % keys[i].size();
      int temp = keys[i][j];
      keys[i][j] = keys[i][idx];
      keys[i][idx] = temp;
    }
  }

  //transfer to multidimensional array of pointeres
  int** arr = new int*[keys.size()];
  bool** results = new bool*[keys.size()];
  for (int i = 0; i < keys.size(); i++) {
    arr[i] = new int[keys[i].size()];
    for (int j = 0; j < keys[i].size(); j++) {
      arr[i][j] = keys[i][j];
    }
    results[i] = new bool[keys[i].size()]();
  }

  //insertion
  int totalSize = 0;
  cout << "Inserting" << endl;
  for (int i = 0; i < keys.size(); i++) {
    tree.insert(arr[i], results[i], keys[i].size());
    for (int j = 0; j < keys[i].size(); j++) {
      if (results[i][j] != true) {
        cout << "Did not insert properly" << endl;
      }
    }
    //ensure insertion fails each time again
    tree.insert(arr[i], results[i], keys[i].size());
    for (int j = 0; j < keys[i].size(); j++) {
      if (results[i][j] == true) {
        cout << "inserted an old value / algorithm incorrect" << endl;
      }
    }
    totalSize += keys[i].size();
    //check size
    if (tree.getSize() != totalSize) {
      cout << "Incorrect size algorithm" << endl;
    }
  }

  tree.inOrderTraversal();

  vector<int> sortedKeys;
  for (int i = 0; i < keys.size(); i++) {
    for (int j = 0; j < keys[i].size(); j++) {
      sortedKeys.push_back(keys[i][j]);
    }
  }
  sort(sortedKeys.begin(), sortedKeys.end());
  //check that all elements are sorted in the list and in proper location
  for (int i = 0; i < totalSize; i++) {
    if (sortedKeys[i] != tree.getElement(i)) {
      cout << "not sorted properly" << endl;
    }
  }

  cout << "Finding Elements" << endl;
  for (int i = 0; i < keys.size(); i++) {
    bool *findResults = new bool[keys[i].size()];
    tree.lookup(arr[i], findResults, keys[i].size());
    for (int j = 0; j < keys[i].size(); j++) {
      if (findResults[j] != true) {
        cout << "Did not find the value " << keys[i][j] << endl;
      }
    }
    delete [] findResults;
  }

  cout << "Deleting" << endl;
  int deletions = 0;
  for (int i = 0; i < keys.size(); i++) {
    //check size after each iterative deletion
    if (totalSize - deletions != tree.getSize()) {
      cout << "did not delete properly" << endl;
    }
    //remove all elements, make sure they are all removed
    bool *findResults = new bool[keys[i].size()];
    tree.remove(arr[i], findResults, keys[i].size());
    for (int j = 0; j < keys[i].size(); j++) {
      if (findResults[j] != true) {
        cout << "Could not delete the value " << keys[i][j];
      }
    }
    deletions += keys[i].size();
    //ensure no remaining elements
    tree.lookup(arr[i], findResults, keys[i].size());
    for (int j = 0; j < keys[i].size(); j++) {
      if (findResults[j] == true) {
        cout << "Did not delete the value " << keys[i][j] << endl;
      }
    }
    //ensure removal fails each time again
    tree.remove(arr[i], findResults, keys[i].size());
    for (int j = 0; j < keys[i].size(); j++) {
      if (findResults[j] == true) {
        cout << "Did not delete an old value / algorithm incorrect" << endl;
      }
    }
    //check consistency of tree
    int oldElement = tree.getElement(0);
    for (int i = 1; i < tree.getSize(); i++) {
      int newElement = tree.getElement(i);
      if (newElement <= oldElement) {
        cout << "not consisent tree" << endl;
      }
      oldElement = newElement;
    }
    delete [] findResults;
  }
  cout << "TESTS COMPLETED" << endl;
  tree.inOrderTraversal();
}
