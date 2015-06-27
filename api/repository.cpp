//===----------------------------------------------------------------------===//
//
// NAME         : Repository
// COPYRIGHT    : (c) 2013 Sean Donnellan. All Rights Reserved.
// LICENSE      : The MIT License (see LICENSE.txt for details)
//
//===----------------------------------------------------------------------===//

#include "repository.hpp"

#include "libgit2/include/git2.h"

static const char* repositoriesPath = "D:/vcs";

git::Repository::Repository(const std::string& name)
: myPath(repositoriesPath + ("/" + name)),
  myRepository(nullptr)
{
  int error = git_repository_open(&myRepository, myPath.c_str());
  if (error != 0)
  {
    const git_error* lastError = giterr_last();
    if (lastError && lastError->message)
    {
      fprintf(stderr, "%s\n", lastError->message);
    }
    else
    {
      fprintf(stderr, "Could not open repository: cause unknown.\n");
    }
    return;
  }
}

git::Repository::~Repository()
{
  git_repository_free(myRepository);
  myRepository = nullptr;
}

git_object* git::Repository::Parse(const std::string& specification)
{
  git_object* object = nullptr;
  int error = git_revparse_single(&object, myRepository, specification.c_str());
  if (error != 0)
  {
    const git_error* lastError = giterr_last();
    if (lastError && lastError->message)
    {
      fprintf(stderr, "%s\n", lastError->message);
    }
    else
    {
      fprintf(stderr, "Could not resolve '%s'\n", specification.c_str());
    }
    return nullptr;
  }
  return object;
}

//===--------------------------- End of the file --------------------------===//
