#pragma once

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2020 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

#include <map>
#include <string>
#include <vector>

using namespace std;

class Parser;


class ParseNode {
public:
  virtual bool isVariable() const {return false;}
  virtual int evaluate(const Parser &parser) const = 0;
  virtual void assign(const Parser &parser, int value) const;
  virtual void assignVector(const Parser &parser, const vector<int> &value) const;
 
  virtual ~ParseNode() = 0;
};


class Parser {
  enum Operator {
    OpNone,
    OpPlus, // 1
    OpMinus, // 1
    OpTimes, // 2
    OpDivide, // 2
    OpMod, // 2
    OpMax,
    OpMin,

    OpEquals,
    OpNotEquals,
    OpLess,
    OpMore,
    OpLessEquals,
    OpMoreEquals,

    OpAnd,
    OpOr,

    OpNot,

    OpAssign,

    OpLeftP,
    OpRightP,

    OpInc,
    OpDec,
    OpIncPost,
    OpDecPost,
    OpIncPre,
    OpDecPre,

    OpReturn,
    OpSize,
    OpSizeBase,
    OpSizeSub,
    OpBreak,
    OpSortArray,
  };


  static const int levelMax = 3;
  static int getLevel(Operator op);
  static void eatWhite(const string &expr, size_t &pos);

  ParseNode *parseFunction(const string &name, const string &expr, size_t &pos);


  static Operator parseOperator(const string &expr, size_t &pos, int level);
  static Operator parseOperatorAux(const string &expr, size_t &pos);

  struct Symbol {
    string desc;
    bool isVector;
    bool isMatrix;
    vector< vector<int> > value;
  };

  map<string, Symbol> symb;
  mutable map<string, vector<int> > var;

  mutable int breakMode;
  mutable bool returnMode;
  mutable bool ignoreValue;

  ParseNode *parseStatement(const string &expr, bool primary);
  ParseNode *parseStatement(ParseNode *left, const string &expr, size_t &pos, int level, bool changeSign);

  class StatementNode : public ParseNode {
    ParseNode *node;
    StatementNode *next;

    StatementNode(const StatementNode &); //Do not use
    StatementNode &operator=(const StatementNode &); //Do not use

  public:
    int evaluate(const Parser &parser) const;

    StatementNode();
    virtual ~StatementNode();
    friend class Parser;
  };

  class ValueNode : public ParseNode {
    string value;
    ValueNode(const ValueNode &); //Do not use
    ValueNode &operator=(const ValueNode &); //Do not use

  public:
    int evaluate(const Parser &parser) const;
    bool isVariable() const;
    void assign(const Parser &parser, int value) const;
    void assignVector(const Parser &parser, const vector<int> &value) const;
 
    ValueNode();
    virtual ~ValueNode();
    friend class Parser;
  };

  class ArrayValueNode : public ParseNode {
    string expr;
    ParseNode *index;
    ParseNode *index2;    
    ArrayValueNode(const ArrayValueNode &); //Do not use
    ArrayValueNode &operator=(const ArrayValueNode &); //Do not use

  public:
    int evaluate(const Parser &parser) const;
    void assign(const Parser &parser, int value) const;
    void assignVector(const Parser &parser, const vector<int> &value) const;
 
    bool isVariable() const;
    ArrayValueNode();
    virtual ~ArrayValueNode();
    friend class Parser;
  };



  class UnaryOperatorNode : public ParseNode {
    Operator op;
    ParseNode *right;
    UnaryOperatorNode(const UnaryOperatorNode &); //Do not use
    UnaryOperatorNode &operator=(const UnaryOperatorNode &); //Do not use

  public:
    int evaluate(const Parser &parser) const;

    UnaryOperatorNode();
    virtual ~UnaryOperatorNode();
    friend class Parser;
  };

  class BinaryOperatorNode : public ParseNode {
    Operator op;
    ParseNode *left;
    ParseNode *right;
    BinaryOperatorNode(const BinaryOperatorNode &); //Do not use
    BinaryOperatorNode &operator=(const BinaryOperatorNode &); //Do not use

  public:
    int evaluate(const Parser &parser) const;

    BinaryOperatorNode();
    virtual ~BinaryOperatorNode();
    friend class Parser;
  };

  class IfNode : public ParseNode {
    ParseNode *condition;
    ParseNode *iftrue;
    ParseNode *iffalse;
    IfNode(const IfNode &); //Do not use
    IfNode &operator=(const IfNode &); //Do not use

  public:
    int evaluate(const Parser &parser) const;

    IfNode();
    virtual ~IfNode();
    friend class Parser;
  };

  class WhileNode : public ParseNode {
    ParseNode *condition;
    ParseNode *body;
    WhileNode(const WhileNode &); //Do not use
    WhileNode &operator=(const WhileNode &); //Do not use

  public:
    int evaluate(const Parser &parser) const;

    WhileNode();
    virtual ~WhileNode();
    friend class Parser;
  };


  class ForNode : public ParseNode {
    ParseNode *start;
    ParseNode *condition;
    ParseNode *update;

    ParseNode *body;
    ForNode(const ForNode &); //Do not use
    ForNode &operator=(const ForNode &); //Do not use

  public:
    int evaluate(const Parser &parser) const;

    ForNode();
    virtual ~ForNode();
    friend class Parser;
  };


  int evaluate(const string &input) const;
  int evaluate(const string &input, int index, int index2) const;
  int evaluateSize(const string &input, int index) const;
  void sortArray(const string &input) const;

  
  void storeVariable(const string &input, const vector<int> &value) const;
  void storeVariable(const string &input, int value) const;
  void storeVariable(const string &input, int index, int value) const;

  UnaryOperatorNode *getUnary();
  BinaryOperatorNode *getBinary();
  ValueNode *getValue();
  ArrayValueNode *getArrayValue();
  StatementNode *getStatement();
  IfNode *getif ();
  WhileNode *getWhile();
  ForNode *getFor();

  ParseNode *parseif (const string &expr, size_t &pos);
  ParseNode *parseReturn(const string &expr, size_t &pos);
  ParseNode *parseValue(const string &word, const string &expr, size_t &pos);
  ParseNode *parseWhile(const string &expr, size_t &pos);
  ParseNode *parseFor(const string &expr, size_t &pos);

  string parseMethod(const string &expr, size_t &pos);

  vector<ParseNode *> nodes;
  bool isMatrix(const string &symbol) const;
  bool isVector(const string &symbol) const;

  const vector<int> &getVector(const string &symbol, int index) const;
public:
  ParseNode *parse(const string &expr);
  static void test();

  Parser();
  ~Parser();
  void clear();
  void addSymbol(const char *name, const string &value);
  void addSymbol(const char *name, int value);
  void addSymbol(const char *name, const vector<string> &value);
  void addSymbol(const char *name, const vector<int> &value);
  void addSymbol(const char *name, vector< vector<int> > &value);
  void removeSymbol(const char *name);
  void declareSymbol(const char *name, const string &desc, bool isVector, bool isMatrix = false);
  void clearSymbols();
  void clearVariables() const;

  void takeVariable(const char*name, vector<int> &val) const;

  void getSymbols(vector< pair<wstring, size_t> > &symb) const;

  void getSymbolInfo(int ix, wstring &name, wstring &desc) const;

  void dumpVariables(gdioutput &gdi, int c1, int c2) const;
  void dumpSymbols(gdioutput &gdi, int c1, int c2) const;
};
