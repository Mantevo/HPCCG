#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include "YAML_Element.hpp"
using namespace std;

/**
 * A constructor for a YAML element.
 *
 * @param key_arg The string value of the element's key.
 * @param value_arg The string value of the element's value.
 */
YAML_Element::YAML_Element(const std::string& key_arg, const std::string& value_arg){
  key = key_arg;
  value = value_arg;
}

/**
 * A destructor for a YAML element.
 */
YAML_Element::~YAML_Element(){
  for (size_t i=0; i<children.size(); i++) {
    delete children[i];
  }
  children.clear();
}

/**
 * Add a new element to the vector of YAML elements.
 *
 * QUESTION: if an element is not added because the key already exists,
 * will this lead to memory leakage?
 *
 * @param key_arg The element key to add.
 * @param value_arg The element value of type `double` to add.
 * @return The added YAML element.
 */
YAML_Element* YAML_Element::add(const std::string& key_arg, double value_arg) {
  this->value = "";
  string converted_value = convert_double_to_string(value_arg);
  YAML_Element* element = new YAML_Element(key_arg,converted_value);
  children.push_back(element);
  return element;
}

/**
 * Add a new element to the vector of YAML elements.
 *
 * @param key_arg The element key to add.
 * @param value_arg The element value of type `int` to add.
 * @return The added YAML element.
 */
YAML_Element* YAML_Element::add(const std::string& key_arg, int value_arg) {
  this->value = "";
  string converted_value = convert_int_to_string(value_arg);
  YAML_Element* element = new YAML_Element(key_arg,converted_value);
  children.push_back(element);
  return element;
}

#ifndef MINIFE_NO_LONG_LONG

/**
 * Add a new element to the vector of YAML elements.
 *
 * @param key_arg The element key to add.
 * @param value_arg The element value of type `long long` to add.
 * @return The added YAML element.
 */
YAML_Element* YAML_Element::add(const std::string& key_arg, long long value_arg) {
  this->value = "";
  string converted_value = convert_long_long_to_string(value_arg);
  YAML_Element* element = new YAML_Element(key_arg,converted_value);
  children.push_back(element);
  return element;
}

#endif

/**
 * Add a new element to the vector of YAML elements.
 *
 * @param key_arg The element key to add.
 * @param value_arg The element value of type `size_t` to add.
 * @return The added YAML element.
 */
YAML_Element* YAML_Element::add(const std::string& key_arg, size_t value_arg) {
  this->value = "";
  string converted_value = convert_size_t_to_string(value_arg);
  YAML_Element* element = new YAML_Element(key_arg,converted_value);
  children.push_back(element);
  return element;
}

/**
 * Add a new element to the vector of YAML elements.
 *
 * @param key_arg The element key to add.
 * @param value_arg The element value of type `std::string&` to add.
 * @return The added YAML element.
 */
YAML_Element* YAML_Element::add(const std::string& key_arg, const std::string& value_arg) {
  this->value = "";
  YAML_Element* element = new YAML_Element(key_arg, value_arg);
  children.push_back(element);
  return element;
}

/**
 * Return a pointer to the YAML element with the specified key if it exists in
 * the vector of YAML elements.
 *
 * @note I, cam, believe an exception should be thrown if there is no
 * element in the vector for the specified key
 *
 * @param key_arg The key of the YAML element to search the vector for.
 * @return A pointer to the YAML element if the key is in the vector, otherwise
 * 0.
 */
YAML_Element* YAML_Element::get(const std::string& key_arg) {
  for (size_t i=0; i<children.size(); i++) {
    if(children[i]->getKey() == key_arg){
      return children[i];
    }
  }
  return 0;
}

/**
 * Print a line of a YAML document.
 *
 * Correct YAML depends on correct spacing; the parameter space should be the
 * proper amount of space for the parent element
 *
 * @param space A string containing the correct amount of spacing for the YAML
 * schema.
 * @return A string containing the YAML representation of a YAML element.
 */
string YAML_Element::printYAML(std::string space){
  string yaml_line = space + key + ": " + value + "\n";
  for(int i=0; i<2; i++) space = space + " ";
  for(size_t i=0; i<children.size(); i++){
    yaml_line = yaml_line + children[i]->printYAML(space);
  }
  return yaml_line;
}

/**
 * Convert a double to a string.
 *
 * @param value_arg The double to convert to a string.
 * @return The string representation of the double.
 */
string YAML_Element::convert_double_to_string(double value_arg){
  stringstream strm;
  strm << value_arg;
  return strm.str();
}

/**
 * Convert an integer to a string.
 *
 * @param value_arg The integer to convert to a string.
 * @return The string representation of the integer.
 */
string YAML_Element::convert_int_to_string(int value_arg){
  stringstream strm;
  strm << value_arg;
  return strm.str();
}

#ifndef MINIFE_NO_LONG_LONG

/**
 * Convert a `long long` to a string.
 *
 * @param value_arg The `long long` to convert to a string.
 * @return The string representation of the `long long`.
 */
string YAML_Element::convert_long_long_to_string(long long value_arg){
  stringstream strm;
  strm << value_arg;
  return strm.str();
}

#endif

/**
 * Convert a `size_t` to a string.
 *
 * @param value_arg The `size_t` to convert to a string.
 * @return The string representation of the `size_t`.
 */
string YAML_Element::convert_size_t_to_string(size_t value_arg){
  stringstream strm;
  strm << value_arg;
  return strm.str();
}
