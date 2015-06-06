#ifndef JSON_WRITER_HPP_
#define JSON_WRITER_HPP_
//===----------------------------------------------------------------------===//
//
// NAME         : JsonWriter
// COPYRIGHT    : (c) 2013 Sean Donnellan. All Rights Reserved.
// LICENSE      : The MIT License (see LICENSE.txt for details)
// DESCRIPTION  :
//
// Provides an easy way for outputting JSON to a charachter stream
// (std::ostream), without first storing all the data and then serialising it.
//
// Usage:
// {
//   JsonWriterObject objectWriter(&std::cout);
//   objectWriter["name"] = "Bill Gates";
//   objectWriter["likes"].array() << "software" << "money" << "helping;
//   {
//      auto o = objectWriter["address"].object();
//      o["city"] = "Medina";
//      o["state"] = "Washington";
//      o["country"] = "United States";
//   }
// }
//
//  Output:
//  {
//    "name": "Bill Gates",
//    "likes": [
//      "software",
//      "money",
//      "helping"
//    ],
//    "address": {
//      "city": "Medina",
//      "state": "Washington",
//      "country": "United States."
//    }
//  }
//
// Concepts:
//   The object and array writers use scope-bound management, to decide when
//   close the object and array.
//
// Known shortcomings:
//   Nothing is put in place to ensure you don't forgot to close a nested
//   object or array.  For example if you use ow["hello"].object() then use
//   os["world"] = "hello", it will not automatically close the "hello" object
//   meaning the output will be malformed.
//
//   The following will compile but will do nothing.
//   {
//     JsonWriterObject ow(&std::cout);
//     ow = "hello";
//   }
//
//   There is currently no support for anything other than array, objects and
//   strings.
//
//===----------------------------------------------------------------------===//

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

class JsonWriterObject;
class JsonWriterArray;

namespace JsonWriter
{
  JsonWriterObject object(std::ostream* output);
  JsonWriterArray array(std::ostream* output);

  // Escapes double quotes, backslash, whitespace (backspace, form-feed, line
  // feed, carriage-return and tab) and all control codes less than 20.
  std::string escape(const char* string);
}

class JsonWriterObject
{
  enum State { WaitingForKey, WaitingForValue, WaitingForAnotherKey, Moved };

  std::ostream& myOutput;
  State myState;
  bool isIndenting;
  std::string myIndentation;

  // This is a helper function used internally to write an integer to the
  // output stream (myOutput) based on the state (myState).
  template<typename T>
  JsonWriterObject& operator <<(T value);

  JsonWriterObject(const JsonWriterObject&); /* = delete; */
  JsonWriterObject& operator =(const JsonWriterObject&); /* = default; */
public:
  JsonWriterObject(std::ostream* output, std::string indentation = "");
  JsonWriterObject(JsonWriterObject&& writer);
  ~JsonWriterObject();

  // This should be called twice (once for the key, and again for the value).
  JsonWriterObject& operator <<(const char* value);

  JsonWriterObject& operator [](const char* key);

  JsonWriterObject& operator =(const std::string& value);
  JsonWriterObject& operator =(const char* value);
  JsonWriterObject& operator =(const unsigned int value);
  JsonWriterObject& operator =(const std::int64_t value);
  JsonWriterObject& operator =(const JsonWriterArray& value);

  // These should really only be on the class returned by operator [].
  JsonWriterArray array();
  JsonWriterObject object();
};

class JsonWriterArray
{
  std::ostream& myOutput;
  bool hasAnElement;
  bool hasBeenMoved;
  bool isIndenting;
  std::string myIndentation;

  JsonWriterArray();

  JsonWriterArray(const JsonWriterArray&); /* = delete; */
  JsonWriterArray& operator =(const JsonWriterArray&); /* = default; */
public:

  JsonWriterArray(std::ostream* output, std::string indentation = "");
  JsonWriterArray(JsonWriterArray&& writer);

  ~JsonWriterArray();

  JsonWriterArray& operator <<(const char* value);
  JsonWriterArray& operator <<(const std::string& value);
  JsonWriterArray& operator <<(const std::vector<std::string>& strings);

  JsonWriterObject object();

  // TODO: Support putting an array in an array.
};

//===--------------------------- End of the file --------------------------===//
#endif
