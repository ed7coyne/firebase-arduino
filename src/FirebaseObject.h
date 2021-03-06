//
// Copyright 2015 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef FIREBASE_OBJECT_H
#define FIREBASE_OBJECT_H

#include "third-party/arduino-json-5.3/include/ArduinoJson.h"

#ifndef FIREBASE_JSONBUFFER_SIZE
#define FIREBASE_JSONBUFFER_SIZE 200
#endif // FIREBASE_JSONBUFFER_SIZE

/**
 * Represents value stored in firebase, may be a singular value (leaf node) or
 * a tree structure.
 */
class FirebaseObject {
 public:
  /**
   * Construct from json.
   * \param data Json formatted string.
   */
  FirebaseObject(const String& data);

  /**
   * Interpret result as a bool, only applicable if result is a single element
   * and not a tree.
   */
  operator bool();

  /**
   * Interpret result as a int, only applicable if result is a single element
   * and not a tree.
   */
  operator int();

  /**
   * Interpret result as a float, only applicable if result is a single element
   * and not a tree.
   */
  operator float();

  /**
   * Interpret result as a String, only applicable if result is a single element
   * and not a tree.
   */
  operator const String&();

  /**
   * Interpret result as a JsonObject, if the result is a tree use this or the
   * operator[] methods below.
   */
  operator const JsonObject&();

  //TODO(proppy): Add comments to these.
  JsonObjectSubscript<const char*> operator[](const char* key);
  JsonObjectSubscript<const String&> operator[](const String& key);
  JsonVariant operator[](JsonObjectKey key) const;
 private:
  String data_;
  StaticJsonBuffer<FIREBASE_JSONBUFFER_SIZE> buffer_;
  JsonObject* json_;
};

#endif // FIREBASE_OBJECT_H
