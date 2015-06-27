#ifndef REPOSITORY_HPP_
#define REPOSITORY_HPP_
//===----------------------------------------------------------------------===//
//
// NAME         : Repository
// COPYRIGHT    : (c) 2013 Sean Donnellan. All Rights Reserved.
// LICENSE      : The MIT License (see LICENSE.txt for details)
// DESCRIPTION  : Provides an abstraction of a libgit2 repository.
//
//===----------------------------------------------------------------------===//

#include <string>

struct git_repository;
struct git_object;

namespace git
{
  class Repository
  {
    std::string myPath;
    git_repository* myRepository;

  public:
    //  Opens the repository with the given name in repositoriesPath..
    Repository(const std::string& name);

    // Closes the repository. Everything opened from the repository should be
    // closed (freed) before this occurs.
    ~Repository();
  
    // Convenience operator to allow instances of this class to be passed
    // directly to the libgit2 functions.
    operator git_repository*() const { return myRepository; }

    // Determines if the repository is opened.
    bool IsOpen() const { return myRepository != nullptr; }

    // Finds an object with the given "specification" which may be the hex-hash
    // or a named reference (tag).
    //
    // The caller is responsnible for freeing the object, and should do so
    // before the Repository destructs.
    //
    // Returns null if it can't open the object.
    git_object* Parse(const std::string& specification);
  };
}

//===--------------------------- End of the file --------------------------===//
#endif
