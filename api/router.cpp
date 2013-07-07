//===----------------------------------------------------------------------===//
//
// NAME         : Router
// COPYRIGHT    : (c) 2013 Sean Donnellan. All Rights Reserved.
// LICENSE      : The MIT License (see LICENSE.txt for details)
//
//===----------------------------------------------------------------------===//

#include "router.hpp"

#include <iostream>
#include <string>
#include <map>
#include <vector>

Router & Router::operator [](const char* term)
{
  auto route = myRoutes.insert(std::make_pair(std::string(term), Router()));
  return route.first->second;
}

bool Router::operator()(const char* term) const
{
  // :TODO: Fix this so it doesn't die if there were placeholders.
  auto result = myRoutes.find(term);
  if (result != myRoutes.end())
  {
    if (result->second.mySelfFunction)
    {
      result->second.mySelfFunction();
      return true;
    }
  }
  return false;
}

bool Router::operator()(const std::vector<std::string>& terms) const
{
  std::vector<std::string> placeholders;

  // True if the last term is a place holder.
  bool lastTermIsPlaceholder = false;
  const Router* router = this;
  for (auto term = std::begin(terms); term != std::end(terms); ++term)
  {
    // The current router takes an place-holder, so the "term" could be
    // anything.
    if (router->myPlaceholderFunction)
    {
      placeholders.push_back(*term);
      ++term;

      // The last term was just a place-holder so stop.
      if (term == std::end(terms))
      {
        lastTermIsPlaceholder = true;
        break;
      }

      // Fall through, as the term shold be looked up in myRoutes now.
    }

    const auto result = router->myRoutes.find(*term);
    if (result == router->myRoutes.end())
    {
      // This probably needs to test if its another place-holder in support of
      // allowing two of them in a row.
      //
      // TODO: Throw an "unknown_route_exception".
      return false;
    }
    else
    {
      router = &result->second;
    }
  }

  if (placeholders.empty() && router->mySelfFunction)
  {
    router->mySelfFunction();
    return true;
  }
  else if (lastTermIsPlaceholder && router->myPlaceholderFunction)
  {
    router->myPlaceholderFunction(placeholders);
    return true;
  }
  else if (router->myPlaceholderSelfFunction)
  {
    router->myPlaceholderSelfFunction(placeholders);
    return true;
  }
  else
  {
    // throw an exception unknown_route_exception
    // for now just return false;
    return false;
  }
}

RouterWithPlaceholder Router::operator [](placeholder_t)
{
  return RouterWithPlaceholder(*this, true);
}

Router & Router::operator =(CallFunction function)
{
  mySelfFunction = function;
  return *this;
}

RouterWithPlaceholder &
RouterWithPlaceholder::operator =(Router::CallPlaceholderFunction function)
{
  if (isPlaceholder)
  {
    myRouter.myPlaceholderFunction = function;
  }
  else
  {
    myRouter.myPlaceholderSelfFunction = function;
  }
  return *this;
}


#ifdef ROUTER_ENABLE_TESTING

void api_information() { puts(__FUNCSIG__); }
void repositories_list() { puts(__FUNCSIG__); }
void repository_information(const std::vector<std::string>& arguments)
{
  std::cout << "repository_information for \""  << arguments[0] << '"'
            << std::endl;
}

void repository_tags(const std::vector<std::string>& arguments)
{ puts(__FUNCSIG__); }

void repository_branches(const std::vector<std::string>& arguments)
{ puts(__FUNCSIG__); }

void repository_tag(const std::vector<std::string>& arguments)
{
  std::cout << "tag \"" << arguments[1] << "\" for \""  << arguments[0] << '"'
            << std::endl;
}

int main()
{
  Router router;
  router["api"] = api_information;
  router["api"]["repos"] = repositories_list;
  router["api"]["repos"][Router::placeholder] = repository_information;
  router["api"]["repos"][Router::placeholder]["tags"] = repository_tags;
  router["api"]["repos"][Router::placeholder]["branches"] = repository_branches;

  router["api"]["repos"][Router::placeholder]["tags"][Router::placeholder] =
    repository_tag;

  // TODO: Support setting up an alias.
  // router["api"]["repositories"] = router["api"]["repos"];

  // This needs working...
  router("api");

  // TODO: varadic arguments would be nice to support router("api", "repos")
  std::vector<std::string> arguments;
  arguments.push_back("api");
  arguments.push_back("repos");
  arguments.push_back("anything");
  arguments.push_back("tags");
  arguments.push_back("something");
  router(arguments);

  return 0;
}
#endif

//===--------------------------- End of the file --------------------------===//