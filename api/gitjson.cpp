//===----------------------------------------------------------------------===//
//
// NAME         : gitjson
// COPYRIGHT    : (c) 2013 Sean Donnellan. All Rights Reserved.
// LICENSE      : The MIT License (see LICENSE.txt for details)
//
//===----------------------------------------------------------------------===//

#include "router.hpp"
#include "jsonwriter.hpp"

#include <ctime>
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

namespace git
{
  // Returns the paths of the repositories at the given Path.
  std::vector<boost::filesystem::path> repositories(
    const boost::filesystem::path& Path);

  // Class for helping set-up the repository
  class Repository
  {
    boost::filesystem::path myPath;
    git_repository* myRepository;

  public:
    //  Opens the repository with the given name in repositoriesPath..
    Repository(const std::string& name);

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

    // Closes the repository. Everything opened from the repository should be
    // closed (freed) before this occurs.
    ~Repository();
  };
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

git::Repository::Repository(const std::string& name)
: myPath(boost::filesystem::path(repositoriesPath) / name),
  myRepository(nullptr)
{
  const boost::filesystem::path path(repositoriesPath);
  if (!boost::filesystem::is_directory(path))
  {
    fprintf(stderr, "Could not find repository as \"%s\" is not a directory.\n",
            repositoriesPath);
    return;
  }
  else if (!boost::filesystem::is_directory(myPath))
  {
    fprintf(stderr, "Could not find repository from 's'\n",
            myPath.string().c_str());
    return;
  }

  int error = git_repository_open(&myRepository, myPath.string().c_str());
  if (error != 0)
  {
    const git_error* lastError = giterr_last();
    if (lastError && lastError->message)
    {
      fprintf(stderr, lastError->message);
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

static int for_tags(
  const char *name, git_oid *oid, void *payload)
{
  std::vector<std::pair<std::string, git_oid>>* tags =
    reinterpret_cast<std::vector<std::pair<std::string, git_oid>>*>(payload);
  tags->push_back(std::make_pair(name, *oid));
  return 0;
}

static const std::string& base_uri()
{
  static const char* env = std::getenv("BASE_URI");
  static const std::string uri(env? env: "");
  return uri;
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

  {
    std::vector<boost::filesystem::path> repos = git::repositories(path);
    auto aw = JsonWriter::array(&std::cout);
    for (auto repo = std::begin(repos); repo != std::end(repos); ++repo)
    {
      aw << repo->stem().string();
    }
  }
}

std::vector<std::string> branch_list(git_repository* repository)
{
  std::vector<std::string> branchNames;

  // Collect up a list
  int ret;
  git_branch_iterator* iterator = nullptr;
  git_reference* reference = nullptr;
  git_branch_t type;
  git_branch_iterator_new(&iterator, repository, GIT_BRANCH_LOCAL);
  while (ret = git_branch_next(&reference, &type, iterator) != GIT_ITEROVER &&
         ret != 0)
  {
    const char* name = nullptr;
    git_branch_name(&name, reference);
    branchNames.push_back(name);
  }
  git_branch_iterator_free(iterator);
  return branchNames;
}

void repository_information(const std::vector<std::string>& arguments)
{
  const std::string& repositoryName = arguments.front();

  boost::filesystem::path path(repositoriesPath);
  path /= repositoryName;

  git_repository* repo;
  int error = git_repository_open(&repo, path.string().c_str());

  if (error != 0) return;

  std::vector<std::string> branches = branch_list(repo);
  std::vector<std::pair<std::string, git_oid>> tags;
  git_tag_foreach(repo, for_tags, &tags);

  {
    char commitHash[64] = {0};
    auto object = JsonWriter::object(&std::cout);
    object["repository"] = repositoryName;
    object["branches"].array() << branches;
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

  git_repository_free(repo);
}

void repository_refs(const std::vector<std::string>& arguments)
{
  // This function has been developed to output it in the following format:
  // https://developer.github.com/v3/git/refs/
  //
  // Example: https://api.github.com/repos/git/git/git/refs
  const std::string& repositoryName = arguments.front();

  boost::filesystem::path path(repositoriesPath);
  path /= repositoryName;

  git_repository* repository;
  int error = git_repository_open(&repository, path.string().c_str());

  if (error != 0) return;

  {
    char commitHash[64] = { 0 };
    auto aw = JsonWriter::array(&std::cout);

    int ret;
    git_reference* reference = nullptr;
    git_reference_iterator* iterator = nullptr;
    git_reference_iterator_new(&iterator, repository);
    while (ret = git_reference_next(&reference, iterator) != GIT_ITEROVER &&
      ret != 0)
    {
      git_oid_fmt(commitHash, git_reference_target(reference));

      auto tagObject = aw.object();
      tagObject["ref"] = git_reference_name(reference);
      tagObject["url"] = base_uri() + "/api/repos/" + repositoryName + "/" +
        git_reference_name(reference);
      tagObject["sha"] = commitHash;
    }
    git_reference_iterator_free(iterator);
  }

  git_repository_free(repository);
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

  git_repository_free(repo);
}

void repository_branches(const std::vector<std::string>& arguments)
{
  const std::string& repositoryName = arguments.front();

  boost::filesystem::path path(repositoriesPath);
  path /= repositoryName;

  git_repository* repo;
  int error = git_repository_open(&repo, path.string().c_str());

  if (error != 0) return;

  const std::vector<std::string> branches = branch_list(repo);
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

void repository_commit(const std::vector<std::string>& arguments)
{
  const std::string& repositoryName = arguments.front();
  git::Repository repository(repositoryName);

  if (!repository.IsOpen()) return;

  const std::string& specification = arguments[1];

  git_object* object = repository.Parse(specification);
  if (!object) return;

  // TODO: Handle indirection through an annoated tag.
  switch (git_object_type(object))
  {
  default:
    fprintf(stderr, "'%s' does not reference a commit.\n",
	    specification.c_str());
    return;
  case GIT_OBJ_COMMIT:
    break;
  }

  const git_oid* oid = git_object_id(object);
  const git_commit* commit = (const git_commit*)object;

  char commitHash[41];
  commitHash[40] = '\0';
  git_oid_fmt(commitHash, oid);

  const git_signature * author = git_commit_author(commit);
  const git_signature * comitter = git_commit_committer(commit);
  const git_oid* treeOid = git_commit_tree_id(commit);
  {
    char isoDateString[sizeof "2011-10-08T07:07:09Z"];

    auto object = JsonWriter::object(&std::cout);
    {
      const time_t commitTime = git_commit_time(commit);
      const tm* time = std::gmtime(&commitTime);
      std::strftime(isoDateString, sizeof(isoDateString),
        "%Y-%m-%dT%H:%M:%SZ", time);

      auto authorObject = object["author"].object();
      authorObject["date"] = isoDateString;
      authorObject["email"] = author->email;
      authorObject["name"] = author->name;
    }

    {
      const time_t commitTime = comitter->when.time;
      const tm* time = std::gmtime(&commitTime);
      std::strftime(isoDateString, sizeof(isoDateString),
        "%Y-%m-%dT%H:%M:%SZ", time);

      auto authorObject = object["comitter"].object();
      authorObject["date"] = isoDateString;
      authorObject["email"] = comitter->email;
      authorObject["name"] = comitter->name;
    }

    object["message"] = JsonWriter::escape(git_commit_message(commit));
    {
      auto parentsArray = object["parents"].array();
      for (unsigned int i = 0, count = git_commit_parentcount(commit);
           i < count; ++i)
      {
        char parentShaString[GIT_OID_HEXSZ + 1];
        git_oid_tostr(parentShaString, sizeof(parentShaString),
                      git_commit_parent_id(commit, i));

        auto parentObject = parentsArray.object();
        parentObject["sha"] = parentShaString;
        parentObject["url"] = base_uri() + "/api/repos/" + repositoryName +
           "/commits/" + commitHash;;
      }
    }
    object["sha"] = commitHash;

    {
      char treeHash[41];
      treeHash[40] = '\0';
      git_oid_fmt(treeHash, treeOid);

      auto treeObject = object["tree"].object();
      treeObject["sha"] = treeHash;
      treeObject["url"] = base_uri() + "/api/repos/" + repositoryName +
        "/tree/" + treeHash;
    }
    object["url"] = base_uri() + "/api/repos/" + repositoryName + "/commits/" +
      commitHash;
  }

  git_object_free(object);
}

void repository_file(const std::vector<std::string>& arguments)
{
  // Writes out a given file from the repository, as-is, with no additional
  // metadata.
  const std::string& repositoryName = arguments.front();
  git::Repository repository(repositoryName);

  if (!repository.IsOpen()) return;

  const std::string& specification = arguments[1];

  git_object* object = repository.Parse(specification);
  if (!object) return;

  // TODO: Handle other types better.
  if (git_object_type(object) != GIT_OBJ_BLOB)
  {
    fprintf(stderr, "The given reference is not a file.");
    return;
  }

  const git_blob* blob = (const git_blob *)object;

  fwrite(git_blob_rawcontent(blob), git_blob_rawsize(blob), 1, stdout);

  git_object_free(object);
}

void repository_next_command(const std::vector<std::string>& arguments)
{
  const std::string& repositoryName = arguments.front();

  // The goal of the following is to implement "ls-tree" that outputs to JSON.
  {
    git::Repository repository(repositoryName);

    git_object* object = nullptr;
    int error = git_revparse_single(&object, repository, "master");
    if (error)
    {
      fprintf(stderr, "The given reference was bad.");
      return;
    }

    // TODO: Free object.
    const git_oid* objectId = git_object_id(object);

    git_tree* tree = nullptr;

    {
      git_commit* commit = nullptr;
      git_commit_lookup(&commit, repository, objectId);
      error = git_commit_tree(&tree, commit);
      if (error)
      {
        fprintf(stderr, "Looking up tree from commit\n");
      }
      git_commit_free(commit);
    }

    char hash[64] = {0};
    auto files = JsonWriter::array(&std::cout);
    const size_t entryCount = git_tree_entrycount(tree);
    for (int i = 0; i < entryCount; ++i)
    {
      const git_tree_entry* entry = git_tree_entry_byindex(tree, i);
      git_oid_fmt(hash, git_tree_entry_id(entry));

      // Convert the "mode" parameter to base8 number to be the same as the
      // "mode" parameter here, http://developer.github.com/v3/git/trees/
      std::stringstream ss;
      ss << std::oct << git_tree_entry_filemode(entry);

      auto tagObject = files.object();
      tagObject["path"] = git_tree_entry_name(entry);
      tagObject["mode"] = ss.str();
      tagObject["sha"] = hash;
    }

    git_tree_free(tree);
  }
}

int main(int argc, char* argv[])
{
  // Command line parser.
  //
  // examples:
  // /api/repos/<repo-name>/tags
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

  git_threads_init();

  Router router;
  router["api"] = api_information;
  router["api"]["repos"] = repositories_list;
  router["api"]["repos"][Router::placeholder] = repository_information;
  router["api"]["repos"][Router::placeholder]["refs"] = repository_refs;
  router["api"]["repos"][Router::placeholder]["branches"] = repository_branches;
  router["api"]["repos"][Router::placeholder]["tags"] = repository_tags;
  router["api"]["repos"][Router::placeholder]["tags"][Router::placeholder] =
    repository_tag;
  router["api"]["repos"][Router::placeholder]["commits"][Router::placeholder] =
    repository_commit;

  // Output the file with no manipulation (i.e it won't be put into JSON, etc.
  // TODO: Add support for "raw" and change this to use "raw".
  router["api"]["repos"][Router::placeholder]["file"][Router::placeholder] =
    repository_file;

  // A work in progress.
  router["api"]["repos"][Router::placeholder]["next"] = repository_next_command;

  // Perform the route.
  if (!router(argv[1], '/'))
  {
    fprintf(stderr, "Unknown resource: %s\n", uri.c_str());
    git_threads_shutdown();
    return 1;
  }

  git_threads_shutdown();
  return 0;
}

//===--------------------------- End of the file --------------------------===//
