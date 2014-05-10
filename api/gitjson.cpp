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
namespace util
{

  const static char Base64Lookup[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  // Base64 encode the data from Content to Content + Size and insert new lines
  // every 60 characters.
  std::string Base64Encode(const void * Content, git_off_t Size, bool NewLines)
;
}

std::string util::Base64Encode(
  const void * Content,
  git_off_t Size,
  bool NewLines)
{
  const char padCharacter('=');

  // Determine how big the output string will need to be.
  size_t encodedSize = ((Size / 3) + (Size % 3 > 0)) * 4;
  if (NewLines) encodedSize += encodedSize / 60;

  // Implementation taken from the C++ version from:
  // http://en.wikibooks.org/wiki/Algorithm_Implementation/Miscellaneous/Base64
  std::string encodedString;
  encodedString.reserve(encodedSize);

  long temp;
  const char* cursor = static_cast<const char*>(Content);

  for (size_t idx = 0, rowCount = 4; idx < Size / 3; ++idx, rowCount += 4)
  {
    temp  = (*cursor++) << 16; //Convert to big endian
    temp += (*cursor++) << 8;
    temp += (*cursor++);
    encodedString.append(1, Base64Lookup[(temp & 0x00FC0000) >> 18]);
    encodedString.append(1, Base64Lookup[(temp & 0x0003F000) >> 12]);
    encodedString.append(1, Base64Lookup[(temp & 0x00000FC0) >> 6 ]);
    encodedString.append(1, Base64Lookup[(temp & 0x0000003F)      ]);

    if (NewLines && rowCount == 60)
    {
      encodedString.append("\\n");
      rowCount = 0;
    }
  }

  switch(Size % 3)
  {
  case 1:
    temp  = (*cursor++) << 16; //Convert to big endian
    encodedString.append(1, Base64Lookup[(temp & 0x00FC0000) >> 18]);
    encodedString.append(1, Base64Lookup[(temp & 0x0003F000) >> 12]);
    encodedString.append(2, padCharacter);
    break;
  case 2:
    temp  = (*cursor++) << 16; //Convert to big endian
    temp += (*cursor++) << 8;
    encodedString.append(1, Base64Lookup[(temp & 0x00FC0000) >> 18]);
    encodedString.append(1, Base64Lookup[(temp & 0x0003F000) >> 12]);
    encodedString.append(1, Base64Lookup[(temp & 0x00000FC0) >> 6 ]);
    encodedString.append(1, padCharacter);
    break;
  }

  return encodedString;
}

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
    fprintf(stderr, "Could not find repository from '%s'\n",
            myPath.string().c_str());
    return;
  }

  int error = git_repository_open(&myRepository, myPath.string().c_str());
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
  while ((ret = git_branch_next(&reference, &type, iterator) != GIT_ITEROVER) &&
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
    while ((ret = git_reference_next(&reference, iterator) != GIT_ITEROVER) &&
      ret != 0)
    {
      auto tagObject = aw.object();
      tagObject["ref"] = git_reference_name(reference);
      tagObject["url"] = base_uri() + "/api/repos/" + repositoryName +
        "/refs/" + git_reference_name(reference);

      auto objectObject = tagObject["object"].object();
      const git_ref_t type = git_reference_type(reference);
      if (type == GIT_REF_OID)
      {
        git_oid_fmt(commitHash, git_reference_target(reference));
        objectObject["sha"] = commitHash;

        const git_oid* const peeled = git_reference_target_peel(reference);
        if (peeled)
        {
          objectObject["type"] = "tag";
          objectObject["url"] = base_uri() + "/api/repos/" + repositoryName +
            "/tags/" + commitHash;

          // This is not part of the GitHub API, but is provided as it didn't
          // require any additional cost to look-up.
          git_oid_fmt(commitHash, peeled);
          objectObject["target_sha"] = commitHash;
        }
        else
        {
          objectObject["type"] = "commit";
          objectObject["url"] = base_uri() + "/api/repos/" + repositoryName +
            "/commits/" + commitHash;
        }
      }
      else if (type == GIT_REF_SYMBOLIC)
      {
        objectObject["target"] = git_reference_symbolic_target(reference);
        objectObject["type"] = "symbolic";
      }
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
        tagObject["url"] = base_uri() + "/api/repos/" + repositoryName +
          "/tags/" + tag->first;
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
  // Implements: https://developer.github.com/v3/git/tags/#get-a-tag
  const std::string& repositoryName = arguments.front();
  git::Repository repository(repositoryName);

  if (!repository.IsOpen()) return;

  // The argument has to be the SHA.
  // TODO: Add validation to ensure it is the correct length/valid etc.
  git_oid objectId;
  int error = git_oid_fromstr(&objectId, arguments[1].c_str());
  if (error)
  {
    // TODO: Handle errors better.
    std::exit(1);
  }

  git_tag *tag = nullptr;
  error = git_tag_lookup(&tag, repository, &objectId);
  if (error != 0 || !tag)
  {
    // TODO: Handle errors better.
    std::exit(2);
  }

  const git_otype type = git_tag_target_type(tag);
  if (type != GIT_OBJ_COMMIT)
  {
    // TODO: Handle errors better.
    std::exit(3);
  }

  char isoDateString[sizeof "2011-10-08T07:07:09Z"];
  const git_signature* const tagger = git_tag_tagger(tag);
  const time_t tagTime = tagger->when.time;
  const tm* const time = std::gmtime(&tagTime);
  std::strftime(isoDateString, sizeof(isoDateString), "%Y-%m-%dT%H:%M:%SZ",
                time);
  {
    auto object = JsonWriter::object(&std::cout);

    object["tag"] = git_tag_name(tag);
    object["sha"] = arguments[1];
    object["url"] = base_uri() + "/api/repos/" + repositoryName +
      "/commits/" + arguments[1];
    object["message"] = JsonWriter::escape(git_tag_message(tag));
    {
      auto taggerObject = object["tagger"].object();
      taggerObject["name"] = tagger->name;
      taggerObject["email"] = tagger->email;
      taggerObject["date"] = isoDateString;

    }

    {
      auto objectObject = object["object"].object();
      objectObject["type"] = "commit";

      char targetShaString[GIT_OID_HEXSZ + 1];
      git_oid_tostr(targetShaString, sizeof(targetShaString),
                    git_tag_target_id(tag));
      objectObject["sha"] = targetShaString;
      objectObject["url"] = base_uri() + "/api/repos/" + repositoryName +
        "/commits/" + targetShaString;
    }
  }

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
        "/trees/" + treeHash;
    }
    object["url"] = base_uri() + "/api/repos/" + repositoryName + "/commits/" +
      commitHash;
  }

  git_object_free(object);
}

void repository_tree(const std::vector<std::string>& arguments)
{
  // Implements: https://developer.github.com/v3/git/trees/#get-a-tree
  // Example:
  //   https://api.github.com/repos/git/git/git/trees/
  //     7f4837766f5bf8bd1d008ac38470a53f34b4f910
  const std::string& repositoryName = arguments.front();
  git::Repository repository(repositoryName);

  if (!repository.IsOpen()) return;

  git_oid objectId;
  int error = git_oid_fromstr(&objectId, arguments[1].c_str());
  if (error)
  {
    // TODO: Handle errors better.
    std::exit(1);
  }

  git_tree* tree = nullptr;
  error = git_tree_lookup(&tree, repository, &objectId);
  if (error)
  {
    // TODO: Handle errors better.
    std::exit(1);
  }

  {
    char shaString[GIT_OID_HEXSZ + 1];
    char isoDateString[sizeof "2011-10-08T07:07:09Z"];

    auto object = JsonWriter::object(&std::cout);
    object["sha"] = arguments[1];
    object["url"] = base_uri() + "/api/repos/" + repositoryName + "/trees/" +
      arguments[1];
    {
      auto treeArray = object["tree"].array();

      const size_t entryCount = git_tree_entrycount(tree);
      for (int i = 0; i < entryCount; ++i)
      {
        const git_tree_entry* entry = git_tree_entry_byindex(tree, i);
        git_oid_tostr(shaString, sizeof(shaString), git_tree_entry_id(entry));

        // Convert the "mode" parameter to base8 number to be the same as the
        // "mode" parameter here, http://developer.github.com/v3/git/trees/
        std::stringstream ss;
        ss << std::oct << git_tree_entry_filemode(entry);

        auto tagObject = treeArray.object();
        tagObject["path"] = git_tree_entry_name(entry);
        tagObject["mode"] = ss.str();
        tagObject["sha"] = shaString;

        // Tree objects in git do not store the size of the blobs, so additional
        // look-ups are required for that.
        //
        // First determine if the item is an blob or a tree.
        if (git_tree_entry_type(entry) == GIT_OBJ_BLOB)
        {
          const git_oid* oid = git_tree_entry_id(entry);
          git_blob* blob = nullptr;
          git_blob_lookup(&blob, repository, oid);
          tagObject["type"] = "blob";
          tagObject["size"] = git_blob_rawsize(blob);
          tagObject["url"] = base_uri() + "/api/repos/" + repositoryName +
            "/blobs/" + shaString;
        }
        else if(git_tree_entry_type(entry) == GIT_OBJ_TREE)
        {
          tagObject["type"] = "tree";
          tagObject["url"] = base_uri() + "/api/repos/" + repositoryName +
            "/trees/" + shaString;
        }
      }
    }
  }
  git_tree_free(tree);
}


void repository_blob(const std::vector<std::string>& arguments)
{
  // Implements: https://developer.github.com/v3/git/blobs/#get-a-blob
  //
  // Example:
  //   https://api.github.com/repos/git/git/git/blobs/
  //     5e98806c6cc246acef5f539ae191710a0c06ad3f
  //
  // NOTE: GitHub's API only supports up to 100 megabytes.
  //
  // The API is suppose to support both applicaiton/json or 'raw' at the moment
  // this is only the "json" one (see /file/ for details on the raw option).
  const std::string& repositoryName = arguments.front();
  git::Repository repository(repositoryName);

  if (!repository.IsOpen()) return;

  git_oid objectId;
  const int error = git_oid_fromstr(&objectId, arguments[1].c_str());
  git_blob* blob = nullptr;
  git_blob_lookup(&blob, repository, &objectId);

  // TODO: Determine if it needs to be base64 encoded.
  const bool base64Encoded = true;

  {
    auto object = JsonWriter::object(&std::cout);

    // TODO: Instead of creating a temporary string for the base64, the output
    // could be written directly to the stream.
    object["content"] = util::Base64Encode(git_blob_rawcontent(blob),
                                           git_blob_rawsize(blob),
                                           true);
     object["encoding"] = (base64Encoded ? "base64" : "utf-8");
    object["sha"] = arguments[1];
    object["url"] = base_uri() + "/api/repos/" + repositoryName +
      "/blobs/" + arguments[1];
    object["size"] = git_blob_rawsize(blob);
  }
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

    git_object_free(object);
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
    fprintf(stderr, "       %s -\n", argv[0]);
    return 1;
  }

  const std::string uri(argv[1]);

  // Check if it starts with /api/
  if (uri.find("/api/", 0, 5) == std::string::npos &&
      uri != "-")
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
  router["api"]["repos"][Router::placeholder]["trees"][Router::placeholder] =
    repository_tree;
  router["api"]["repos"][Router::placeholder]["blobs"][Router::placeholder] =
    repository_blob;

  // Output the file with no manipulation (i.e it won't be put into JSON, etc.
  // TODO: Add support for "raw" and change this to use "raw".
  router["api"]["repos"][Router::placeholder]["file"][Router::placeholder] =
    repository_file;

  // A work in progress.
  router["api"]["repos"][Router::placeholder]["next"] = repository_next_command;

  if (uri == "-")
  {
    // Read the URI from standard in.
    std::string uriFromStandardIn;
    while (!std::getline(std::cin, uriFromStandardIn).eof() &&
           uriFromStandardIn != "\4")
    {
      // Perform the route.
      if (!router(uriFromStandardIn.c_str(), '/'))
      {
        fprintf(stderr, "Unknown resource: %s\n", uri.c_str());
        git_threads_shutdown();
        return 1;
      }
    }
  }
  else
  {
    // Perform the route.
    if (!router(argv[1], '/'))
    {
      fprintf(stderr, "Unknown resource: %s\n", uri.c_str());
      git_threads_shutdown();
      return 1;
    }
  }

  git_threads_shutdown();
  return 0;
}

//===--------------------------- End of the file --------------------------===//
