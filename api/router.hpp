#ifndef ROUTER_HPP_
#define ROUTER_HPP_
//===----------------------------------------------------------------------===//
//
// NAME         : Router
// COPYRIGHT    : (c) 2013 Sean Donnellan. All Rights Reserved.
// LICENSE      : The MIT License (see LICENSE.txt for details)
// DESCRIPTION  :
//
// Provides an simple and easy way of managing the routing from the components
// of a path to a function.
//
// For example:
//   /api/books - You want to map this to a list_books function.
//   /api/books/<name> - You want to map this to an about_book function.
//
// Usage:
//   Router router;
//   router["api"]["books"] = list_books
//   router["api"]["books"][Router::placeholder] = about_book
//
// Concepts:
//   The placeholder is used where you want to allow any string to be used and
//   it will be passed to the function being called.
//
//   In the example above, "placeholder" would be the name of the book.
//
//   Once a placeholder is used, the callback function accepts a std::vector of
//   std::string.
//
//===----------------------------------------------------------------------===//

#include <string>
#include <map>
#include <vector>

class RouterWithPlaceholder;
class Router
{
public:
  typedef void (* CallFunction)(void);
  typedef void (* CallPlaceholderFunction)(const std::vector<std::string>&);

  Router() : mySelfFunction(nullptr), myPlaceholderFunction(nullptr),
             isSunkingUpAllRemainingTerms(false) {}

  // Used to indicate that this route takes a placeholder and passes it to the
  // function.
  typedef int placeholder_t;
  static const int placeholder = 0;

  // Used to indicate that this route takes all the remaining arguments.
  // TODO: Ensure this can only be used once.
  struct placeholder_remaining_t { int v;};
  static const placeholder_remaining_t placeholder_remaining;

  // Methods for configuring the routing information.
  Router & operator =(CallFunction function);
  Router & operator [](const char* term);
  RouterWithPlaceholder operator [](placeholder_t);
  RouterWithPlaceholder operator [](placeholder_remaining_t);

  // Methods for invoking the functions. Returns false is no route is present.
  //
  // The first will tokenise the given path into terms.
  // The second asumes it has already been broken up into terms.
  bool operator()(const char* path, char token = '/') const;
  bool operator()(const std::vector<std::string>& terms) const;

private:
  friend class RouterWithPlaceholder;

  // The function to call
  union
  {
    CallPlaceholderFunction myPlaceholderSelfFunction;
    CallFunction mySelfFunction;
  };

  // The function to call if the next argument is a place holder (match
  // anything) value.
  CallPlaceholderFunction myPlaceholderFunction;

  bool isSunkingUpAllRemainingTerms;

  std::map<std::string, Router> myRoutes;
};

class RouterWithPlaceholder
{
public:
  RouterWithPlaceholder(Router& router,
                        bool isPlaceholder,
                        bool isSunkingUpAllRemainingPlaceholders)
    : myRouter(router),
      isPlaceholder(isPlaceholder),
      isSunkingUpAllRemainingPlaceholders(isSunkingUpAllRemainingPlaceholders)
    {
    }

  // Methods for configuring the routing information.
  RouterWithPlaceholder & operator =(Router::CallPlaceholderFunction function);
  RouterWithPlaceholder operator [](const char* term)
  {
    return RouterWithPlaceholder(myRouter.operator [](term), false, false);
  }
  RouterWithPlaceholder operator [](Router::placeholder_t v)
  {
    return myRouter.operator [](v);
  }

  RouterWithPlaceholder operator [](Router::placeholder_remaining_t v)
  {
    return myRouter.operator [](v);
  }

private:
  RouterWithPlaceholder & operator =(const RouterWithPlaceholder&);

  Router& myRouter;

  bool isPlaceholder;
  bool isSunkingUpAllRemainingPlaceholders;
};

//===--------------------------- End of the file --------------------------===//
#endif
