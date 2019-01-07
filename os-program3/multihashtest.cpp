#include "shash2.h"
#include <vector>
#include <set>
#include <unordered_map>
#include <iostream>
#include <algorithm>
using namespace std;

#define SIZE 20000
#define BUCKETS 64

int main(int argc, char** argv) {
  set<int> map;
  srand(time(NULL));
  for (int i = 0; i < SIZE; i++) {
    map.insert(rand() % (SIZE * 10));
  }
  shash2 hash(BUCKETS);

  //create insertions
  vector<int> elements(map.begin(), map.end());
  vector<vector<int>> keys;
  for (int i = 0; i < SIZE / 64; i++) {
    keys.push_back(vector<int>(elements.begin() + i * 32, elements.begin() + (i + 1) * 32));
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
    hash.insert(arr[i], results[i], keys[i].size());
    for (int j = 0; j < keys[i].size(); j++) {
      if (results[i][j] != true) {
        cout << "Did not insert properly" << endl;
      }
    }
    //ensure insertion fails each time again
    hash.insert(arr[i], results[i], keys[i].size());
    for (int j = 0; j < keys[i].size(); j++) {
      if (results[i][j] == true) {
        cout << "inserted an old value / algorithm incorrect" << endl;
      }
    }
    totalSize += keys[i].size();
  }
  /*
  for (int i = 0; i < BUCKETS; i++) {
    cout << hash.getBucketSize(i) << endl;
    for (int j = 0; j < hash.getBucketSize(i); j++) {
      cout << hash.getElement(i, j) % BUCKETS << " " << hash.getElement(i, j) << endl;
    }
  }
  */
  //check that all elements are in list
  for (int i = 0; i < keys.size(); i++) {
    hash.lookup(arr[i], results[i], keys[i].size());
    for (int j = 0; j < keys[i].size(); j++) {
      if (results[i][j] != true) {
        cout << "Did not insert properly" << endl;
      }
    }
  }
  //check that all elements are sorted in the list and in proper bucket
  for (int i = 0; i < BUCKETS; i++) {
    int oldElement = hash.getElement(i, 0);
    for (int j = 1; j < hash.getBucketSize(i); j++) {
      int val = hash.getElement(i, j);
      if (val % BUCKETS != i) {
        cout << "WRONG INSERTION PLACE" << endl;
      }
      else if (val < oldElement) {
        cout << "NOT SORTED" << endl;
      }
      oldElement = val;
    }
  }
  //redundant, forgot this was here... whatever check again
  cout << "Finding Elements" << endl;
  for (int i = 0; i < keys.size(); i++) {
    bool *findResults = new bool[keys[i].size()];
    hash.lookup(arr[i], findResults, keys[i].size());
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
    int size = 0;
    for (int j = 0; j < BUCKETS; j++) {
      size += hash.getBucketSize(j);
    }
    if (size != totalSize - deletions) {
      cout << "did not delete properly" << endl;
    }
    //remove all elements, make sure they are all removed
    bool *findResults = new bool[keys[i].size()];
    hash.remove(arr[i], findResults, keys[i].size());
    for (int j = 0; j < keys[i].size(); j++) {
      if (findResults[j] != true) {
        cout << "Could not delete the value " << keys[i][j];
      }
    }
    deletions += keys[i].size();
    //ensure consistency of list remains the same
    for (int j = 0; j < BUCKETS; j++) {
      int oldElement = hash.getElement(j, 0);
      for (int k = 1; k < hash.getBucketSize(j); k++) {
        int val = hash.getElement(j, k);
        if (val % BUCKETS != j) {
          cout << "WRONG INSERTION PLACE" << endl;
        }
        else if (val < oldElement) {
          cout << "NOT SORTED" << endl;
        }
        oldElement = val;
      }
    }
    //ensure no remaining elements
    hash.lookup(arr[i], findResults, keys[i].size());
    for (int j = 0; j < keys[i].size(); j++) {
      if (findResults[j] == true) {
        cout << "Did not delete the value " << keys[i][j] << endl;
      }
    }
    //ensure removal fails each time again
    hash.remove(arr[i], findResults, keys[i].size());
    for (int j = 0; j < keys[i].size(); j++) {
      if (findResults[j] == true) {
        cout << "Did not delete an old value / algorithm incorrect" << endl;
      }
    }
    delete [] findResults;
  }

  for (int i = 0; i < BUCKETS; i++) {
    if (hash.getBucketSize(i) != 0) {
      cout << "did not delete everyting" << endl;
    }
  }

  for (int i = 0; i < keys.size(); i++) {
    delete [] arr[i];
    delete [] results[i];
  }
  delete [] arr;
  delete [] results;

}
