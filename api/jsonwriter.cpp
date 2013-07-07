//===----------------------------------------------------------------------===//
//
// NAME         : JsonWriter
// COPYRIGHT    : (c) 2013 Sean Donnellan. All Rights Reserved.
// LICENSE      : The MIT License (see LICENSE.txt for details)
//
//===----------------------------------------------------------------------===//

#include "jsonwriter.hpp"

JsonWriterObject JsonWriter::object(std::ostream* output)
{
  return JsonWriterObject(output);
}

JsonWriterArray JsonWriter::array(std::ostream* output)
{
  return JsonWriterArray(output);
}


JsonWriterObject::JsonWriterObject(std::ostream* output, std::string indenation)
: myOutput(*output),
  myState(WaitingForKey),
  isIndenting(true),
  myIndentation(indenation)
{
  myOutput << "{";
  if (isIndenting) myOutput << '\n';
}

JsonWriterObject::JsonWriterObject(JsonWriterObject&& writer)
: myOutput(writer.myOutput),
  myState(WaitingForKey),
  isIndenting(writer.isIndenting),
  myIndentation(writer.myIndentation)
{
  writer.myState = Moved;
}

JsonWriterObject::~JsonWriterObject()
{
  if (myState == Moved) return;

  if (isIndenting)
  {
    myOutput << std::endl << myIndentation << '}';

    // No indentation means it is the top level so it can decide where to
    // put the new line.
    if (myIndentation.empty()) myOutput << std::endl;
  }
  else
  {
    myOutput << '}' << std::endl;
  }
}

JsonWriterObject& JsonWriterObject::operator <<(const char* value)
{
  if (isIndenting)
  {
    if (myState == WaitingForAnotherKey)
    {
      myOutput << ',' << std::endl;
    }

    if (myState == WaitingForValue)
    {
      myOutput << ": \"" << value << '"';
      myState = WaitingForAnotherKey;
    }
    else
    {
      myOutput << myIndentation << "  \""      << value << '"';
      myState = WaitingForValue;
    }
  }
  else
  {
    if (myState == WaitingForValue)
    {
      myOutput << ':' << '"' << value << '"';
      myState = WaitingForAnotherKey;
    }
    else
    {
      myOutput << '"' << value << '"';
      myState = WaitingForValue;
    }
  }
  return *this;
}

JsonWriterObject& JsonWriterObject::operator [](const char* key)
{
  if (myState == WaitingForKey || myState == WaitingForAnotherKey)
  {
    *this << key;
  }
  else
  {
    // TODO: Close off the previous one with "null";
  }
  return *this;
}

JsonWriterObject& JsonWriterObject::operator =(const std::string& value)
{
  return this->operator =(value.c_str());
}

JsonWriterObject& JsonWriterObject::operator =(const char* value)
{
  // TODO: Return another class.
  if (myState == WaitingForValue)
  {
    *this << value;
  }
  else
  {
    // TODO: Change this to return another class so this isn't an issue.
    // The object is expecting value.
  }

  return *this;
}

JsonWriterObject& JsonWriterObject::operator =(const JsonWriterArray& value)
{
  if (myState == WaitingForAnotherKey)
  {
    if (isIndenting) myOutput << ',' << std::endl;
    else myOutput << ',' ;
  }

  return *this;
}

JsonWriterArray JsonWriterObject::array()
{
  myOutput << ": ";
  myState = WaitingForAnotherKey;
  return JsonWriterArray(&myOutput, myIndentation + "  ");
}

JsonWriterObject JsonWriterObject::object()
{
  myOutput << ": ";
  myState = WaitingForAnotherKey;
  return JsonWriterObject(&myOutput, myIndentation + "  ");
}


JsonWriterArray::JsonWriterArray(
  std::ostream* output, std::string indentation)
: myOutput(*output),
  hasAnElement(false),
  isIndenting(true),
  myIndentation(indentation),
  hasBeenMoved(false)
{
  if (isIndenting)
  {
    myOutput << "[" << std::endl;
  }
  else
  {
    myOutput << "[";
  }
}

JsonWriterArray::JsonWriterArray(JsonWriterArray&& writer)
: myOutput(writer.myOutput),
  hasAnElement(writer.hasAnElement),
  isIndenting(writer.isIndenting),
  myIndentation(writer.myIndentation),
  hasBeenMoved(false)
{
  writer.hasBeenMoved = true;
}

JsonWriterArray::~JsonWriterArray()
{
  if (hasBeenMoved) return;

  if (isIndenting)
  {
    myOutput << std::endl << myIndentation << ']';

    // No indentation means it is the top level so it can decide where to
    // put the new line.
    if (myIndentation.empty()) myOutput << std::endl;
  }
  else
  {
    myOutput << ']';
  }
}

JsonWriterArray& JsonWriterArray::operator <<(const char* value)
{
  if (isIndenting)
  {
    if (hasAnElement)
    {
      myOutput << ',' << std::endl;
    }

    myOutput << myIndentation << "  \""      << value << '"';
    hasAnElement = true;
  }
  else
  {
    if (hasAnElement)
    {
      myOutput << ",\"" << value << '"';
    }
    else
    {
      myOutput << '"' << value << '"';
      hasAnElement = true;
    }
  }
  return *this;
}

JsonWriterArray& JsonWriterArray::operator <<(const std::string& value)
{
  return this->operator <<(value.c_str());
}

JsonWriterObject JsonWriterArray::object()
{
  if (hasAnElement)
  {
    if (isIndenting)
    {
      myOutput << ',' << std::endl << myIndentation << myIndentation;
    }
    else
    {
      myOutput << ',';
    }
  }
  else if (isIndenting)
  {
    myOutput << myIndentation << myIndentation;
  }

  hasAnElement = true;
  return JsonWriterObject(&myOutput, myIndentation + "  ");
}

#ifdef JSONWRITER_ENABLE_TESTING

#include <iostream>

int main()
{

  {
    JsonWriterObject objectWriter(&std::cout);

    // Best style.
    objectWriter["hello"] = "value";

    // Stream style.
    objectWriter << "world" << "baz";
  }

  {
    JsonWriterArray arrayWriter(&std::cout);

    arrayWriter << "apple" << "pear" << "carrot" << "grape";
  }

  // Now try writing an array as a value of an object, as well as an another
  // object.

  {
    JsonWriterObject objectWriter(&std::cout);

    objectWriter["hello"] = "value";
    objectWriter["second"].array() << "apple" << "pear" << "carrot" << "grape";
    objectWriter["third"].object() << "carrot" << "grape";
    objectWriter["world"] = "asgsg";

    auto o = objectWriter["another"].object();
    o["name"] << "Bill Gates";
    o["address"] << "Redmond";
  }

  {
    // Test the example that is in the header.
    JsonWriterObject objectWriter(&std::cout);
    objectWriter["name"] = "Bill Gates";
    objectWriter["likes"].array() << "software" << "money" << "helping";
    {
      auto o = objectWriter["address"].object();
      o["city"] = "Medina";
      o["state"] = "Washington";
      o["country"] = "United States";
    }
  }

  {
    // Test one of the known shortcomings.
    JsonWriterObject ow(&std::cout);
    ow = "hello";
  }

  {
    // Use the convicence functions
    auto o = JsonWriter::object(&std::cout);
    o["city"] = "Medina";
    o["state"] = "Washington";
    o["country"] = "United States";
  }

  return 0;
}
#endif

//===--------------------------- End of the file --------------------------===//