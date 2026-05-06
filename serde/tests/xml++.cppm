module;

#include <libxml++/libxml++.h>

export module xmlpp;

export namespace xmlpp
{
using xmlpp::DomParser;
using xmlpp::SaxParser;
using xmlpp::TextReader;
using xmlpp::Node;
// using xmlpp::CDataNode;
using xmlpp::CommentNode;
using xmlpp::Element;
using xmlpp::EntityDeclaration;
using xmlpp::EntityReference;
using xmlpp::ProcessingInstructionNode;
using xmlpp::TextNode;
using xmlpp::XIncludeEnd;
using xmlpp::XIncludeStart;
using xmlpp::Attribute;
using xmlpp::AttributeDeclaration;
using xmlpp::AttributeNode;
using xmlpp::Document;
using xmlpp::RelaxNGSchema;
// using xmlpp::XSDSchema;

// namespace validators
// {
// class Validator;
// class DTDValidator;
// class RelaxNGValidator;
// class XSDValidator;
// } // namespace validators
} // namespace xmlpp
