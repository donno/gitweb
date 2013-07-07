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

  Router() : mySelfFunction(nullptr), myPlaceholderFunction(nullptr) {}

  // Used to indicate that this route takes a placeholder and passes it to the
  // function.
  typedef int placeholder_t;
  static const int placeholder = 0;

  // Methods for configuring the routing information.
  Router & operator =(CallFunction function);
  Router & operator [](const char* term);
  RouterWithPlaceholder operator [](placeholder_t);

  // Methods for invoking the functions. Returns false is no route is present.
  bool operator()(const char* term) const;
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

  std::map<std::string, Router> myRoutes;
};

class RouterWithPlaceholder
{
public:
  RouterWithPlaceholder(Router& router, bool isPlaceholder)
  : myRouter(router), isPlaceholder(isPlaceholder) {}

  // Methods for configuring the routing information.
  RouterWithPlaceholder & operator =(Router::CallPlaceholderFunction function);
  RouterWithPlaceholder operator [](const char* term)
  {
    return RouterWithPlaceholder(myRouter.operator [](term), false);
  }
  RouterWithPlaceholder operator [](Router::placeholder_t v)
  {
    return myRouter.operator [](v);
  }

private:
  Router& myRouter;

  bool isPlaceholder;
};

//===--------------------------- End of the file --------------------------===//
#endif
