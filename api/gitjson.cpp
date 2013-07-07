//===----------------------------------------------------------------------===//
//
// NAME         : gitjson
// COPYRIGHT    : (c) 2013 Sean Donnellan. All Rights Reserved.
// LICENSE      : The MIT License (see LICENSE.txt for details)
//
//===----------------------------------------------------------------------===//

#include "router.hpp"
#include "jsonwriter.hpp"

#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iterator>

#include "boost/filesystem.hpp"
#include "boost/filesystem/path.hpp"

#include "libgit2/include/git2.h"

// TODO:
// - Support configuring where the repositories are kept.

#define VERSION "0.1.0"

static const char* repositoriesPath = "W:/source";

// Helper functions
JsonWriterArray& operator <<(
  JsonWriterArray& writer, const std::vector<std::string>& strings);

namespace git
{
  // Returns the paths of the repositories at the given Path.
  std::vector<boost::filesystem::path> repositories(
    const boost::filesystem::path& Path);

}

std::vector<boost::filesystem::path> git::repositories(
  const boost::filesystem::path& Path)
{
  std::vector<boost::filesystem::path> repositories;

  if (!boost::filesystem::is_directory(Path))
  {
    fprintf(stderr, "Could not find repositories as %s is not a directory.",
            Path.string().c_str());
    return repositories;
  }

  for (auto folder = boost::filesystem::directory_iterator(Path);
       folder != boost::filesystem::directory_iterator();
       ++folder)
  {
    if (!boost::filesystem::is_directory(folder->status())) continue;

    git_repository* repo;
    int error = git_repository_open(&repo, folder->path().string().c_str());
    git_repository_free(repo);

    if (error == GIT_ENOTFOUND)
    {
      continue; // This path is not a git repository.
    }
    else if (error != 0)
    {
      fprintf(stderr, "Failed to open git repository (%s): error code: %d\n",
              folder->path().string().c_str(), error);
      continue;
    }

    repositories.push_back(folder->path());
  }

  return repositories;
}

static int for_branches(
	const char *branch_name,
	git_branch_t branch_type,
	void *payload)
{
  std::vector<std::string>* branches =
    reinterpret_cast<std::vector<std::string>*>(payload);
  branches->push_back(branch_name);
  return 0;
}

static int for_tags(
  const char *name, git_oid *oid, void *payload)
{
  std::vector<std::pair<std::string, git_oid>>* tags =
    reinterpret_cast<std::vector<std::pair<std::string, git_oid>>*>(payload);
  tags->push_back(std::make_pair(name, *oid));
  return 0;
}

JsonWriterArray& operator <<(
  JsonWriterArray& writer, const std::vector<std::string>& strings)
{
  std::for_each(std::begin(strings), std::end(strings),
                [&writer](const std::string& s) { writer << s; });

  return writer;
}

static void api_information()
{
  int major, minor, rev;
  git_libgit2_version(&major, &minor, &rev);
  std::cout
    << "{" << std::endl
    << "   \"version\": \"" VERSION "\"," << std::endl
    << "   \"libgit2\": { \"version\": \"" << major << '.' << minor << '.'
                                    << rev << "\" }" << std::endl
    << "}" << std::endl;
}

static void repositories_list()
{
  const boost::filesystem::path path(repositoriesPath);
  if (!boost::filesystem::is_directory(path))
  {
    fprintf(stderr, "Could not find repositories as %s is not a directory.",
            repositoriesPath);
    return;
  }

  std::vector<boost::filesystem::path> repos = git::repositories(path);

  {
    auto aw = JsonWriter::array(&std::cout);
    for (auto repo = std::begin(repos); repo != std::end(repos); ++repo)
    {
      aw << repo->string();
    }
  }
}

void repository_information(const std::vector<std::string>& arguments)
{
  std::cout << "repository_information for \"" << arguments[0] << '"'
            << std::endl;

  // TODO: Implement this function.
}

void repository_tags(const std::vector<std::string>& arguments)
{
  const std::string& repositoryName = arguments.front();

  boost::filesystem::path path(repositoriesPath);
  path /= repositoryName;

  git_repository* repo;
  int error = git_repository_open(&repo, path.string().c_str());

  if (error != 0) return;

  std::vector<std::pair<std::string, git_oid>> tags;
  error = git_tag_foreach(repo, for_tags, &tags);
  git_repository_free(repo);

  {
    char commitHash[64] = {0};
    auto object = JsonWriter::object(&std::cout);
    object["repository"] = repositoryName;
    {
      auto aw = object["tags"].array();
      for (auto tag = std::begin(tags); tag != std::end(tags); ++tag)
      {
        git_oid_fmt(commitHash, &tag->second);

        auto tagObject = aw.object();
        tagObject["name"] = tag->first;
        tagObject["hash"] = commitHash;
      }
    }
  }
}

void repository_branches(const std::vector<std::string>& arguments)
{
  const std::string& repositoryName = arguments.front();

  boost::filesystem::path path(repositoriesPath);
  path /= repositoryName;

  git_repository* repo;
  int error = git_repository_open(&repo, path.string().c_str());

  if (error != 0) return;

  std::vector<std::string> branches;
  error = git_branch_foreach(
    repo, GIT_BRANCH_REMOTE, for_branches, &branches);
  git_repository_free(repo);

  {
    auto object = JsonWriter::object(&std::cout);
    object["repository"] = repositoryName;
    object["branches"].array() << branches;
  }
}

void repository_tag(const std::vector<std::string>& arguments)
{
  std::cout << "tag \"" << arguments[1] << "\" for \""  << arguments[0] << '"'
            << std::endl;
}

int main(int argc, char* argv[])
{
  // Command line parser.
  //
  // examples:
  // /api/repositories/<repo-name>/tags
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <uri>\n", argv[0]);
    return 1;
  }

  const std::string uri(argv[1]);

  // Check if it starts with /api/
  if (uri.find("/api/", 0, 5) == std::string::npos)
  {
    fprintf(stderr, "The URI didn't start with /api/");
    return 1;
  }

  Router router;
  router["api"] = api_information;
  router["api"]["repos"] = repositories_list;
  router["api"]["repos"][Router::placeholder] = repository_information;
  router["api"]["repos"][Router::placeholder]["branches"] = repository_branches;
  router["api"]["repos"][Router::placeholder]["tags"] = repository_tags;
  router["api"]["repos"][Router::placeholder]["tags"][Router::placeholder] =
    repository_tag;

  // Perform the route.
  if (!router(argv[1], '/'))
  {
    fprintf(stderr, "Unknown resource: %s\n", uri.c_str());
    return 1;
  }

  return 0;
}
