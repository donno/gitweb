//===----------------------------------------------------------------------===//
//
// NAME         : gitjson
// COPYRIGHT    : (c) 2013 Sean Donnellan. All Rights Reserved.
// LICENSE      : The MIT License (see LICENSE.txt for details)
//
//===----------------------------------------------------------------------===//

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
  std::vector<std::string>* tags =
    reinterpret_cast<std::vector<std::string>*>(payload);
  tags->push_back(name);
  return 0;
}

template <typename Container>
static void output(const Container& container, std::ostream* output)
{
  std::ostream& out = *output;
  out << "[" << std::endl;
  auto itemEnd = std::end(container);
  for (auto item = std::begin(container); item != itemEnd; )
  {
    out << "  \"" << *(item++) << "\"";
    if (item != itemEnd) out << ',';
    out << std::endl;
  }

  out << "]" << std::endl;
}


static int listRepositories(std::ostream* output)
{
  const boost::filesystem::path path(repositoriesPath);
  if (!boost::filesystem::is_directory(path))
  {
    fprintf(stderr, "Could not find repositories as %s is not a directory.",
            repositoriesPath);
    return 1;
  }

  std::vector<boost::filesystem::path> repos = git::repositories(path);
  std::ostream& out = *output;

  out << "[" << std::endl;
  for (auto repo = std::begin(repos); repo != std::end(repos); )
  {
    out << "  \"" << (repo++)->string() << "\"";
    if (repo != std::end(repos))
    {
      out << ',';
    }

    out << std::endl;
  }

  out << "]" << std::endl;

  return 0;
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

  std::vector<std::string> tokens;
  {
    std::istringstream iss(uri);
    std::string token;
    while (std::getline(iss, token, '/'))
    {
      if (token.empty()) continue;
      tokens.push_back(token);
    }
  }

  for (auto u = tokens.begin(); u != tokens.end(); ++u)
  {
    ///std::cout << *u << std::endl;
  }

  // tokens[0] should be api
  // tokens[1] should be repositories
  // tokens[2] is <repository-name>

  if (tokens.size() == 1)
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
  else if (tokens[1] == "repositories" || tokens[1] == "repos")
  {
    if (tokens.size() == 2)
    {
      // List the repositories.
      return listRepositories(&std::cout);
    }
    else
    {
      // Look up information about a given repository.
      const std::string& repositoryName = tokens[2];

      boost::filesystem::path path(repositoriesPath);
      path /= repositoryName;

      if (tokens.size() == 3)
      {
        // List information about the repo.
        std::cout << "NYI: 1" << std::endl;
      }
      else if (tokens[3] == "tags")
      {
        git_repository* repo;
        int error = git_repository_open(&repo, path.string().c_str());

        if (error != 0) return error;

        std::vector<std::string> branches;
        error = git_tag_foreach(repo, for_tags, &branches);
        git_repository_free(repo);

        output(branches, &std::cout);
      }
      else if (tokens[3] == "branches")
      {
        git_repository* repo;
        int error = git_repository_open(&repo, path.string().c_str());

        if (error != 0) return error;

        std::vector<std::string> branches;
        error = git_branch_foreach(
          repo, GIT_BRANCH_REMOTE, for_branches, &branches);
        git_repository_free(repo);

        output(branches, &std::cout);
      }
    }
  }
  else
  {
    fprintf(stderr, "Unknown resource: %s\n", tokens[1].c_str());
    return 1;
  }



  return 0;
}
