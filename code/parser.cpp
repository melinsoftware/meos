/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2025 Melin Software HB

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
#include "stdafx.h"

#include <vector>
#include <set>
#include <map>
#include "parser.h"
#include "meosException.h"
#include "meos_util.h"
#include <cassert>
#include <algorithm>
#include "gdioutput.h"
#include "gdifonts.h"
#include "localizer.h"
extern gdioutput *gdi_main;

static string readWord(const string &expr, size_t &pos) {
  bool pureNum = true;
  bool firstNum = false;
  bool first = true;
  string res;
  while (pos < expr.size() && isalnum(expr[pos])) {
    res.push_back(expr.at(pos));
    if (!isdigit(expr[pos]))
      pureNum = false;
    else if (first)
      firstNum = true;

    first = false;
    pos++;
  }

  if (firstNum && !pureNum)
    throw meosException("Invalid symbol X#" + res);

  return res;
}

static void matchSE(const string &expr, size_t &pos, char start, char end) {
  assert(expr[pos] == start);
  int count = 1;
  pos++;
  while (pos < expr.size() && count>0) {
    if (expr[pos] == start)
      count++;
    else if (expr[pos] == end)
      count--;
    pos++;
  }
  if (count != 0) {
   string ss, ee;
   ss.push_back(start);
   ee.push_back(end);
   throw meosException("Mismatch of X and Y#" + ss + "#" + ee);
  }
}

static void splitStatements(const string &expr, char separator, const map<char, char> &se, vector<string> &statements) {
  size_t p = 0;
  size_t pold = 0;
  while (p < expr.size()) {
    if (expr[p] == separator) {
      string ex2 = trim(expr.substr(pold, p-pold));
      if (!ex2.empty())
        statements.push_back(ex2);
      pold = ++p;
    }
    else if (se.find(expr[p]) != se.end()) {
      int cut = 0;
      //if (pold == p)
      //  cut = 1;

      matchSE(expr, p, expr[p], se.find(expr[p])->second);
      string ex2 = trim(expr.substr(pold+cut, p-pold-2*cut));
      pold = p;

      //if (ex2.back() == separator)
      //  ex2.pop_back();
      if (!ex2.empty())
        statements.push_back(ex2);
    }
    else if (expr[p] == '(') {
      matchSE(expr, p, '(', ')');
    }
    else
      p++;
  }
  if (p-pold > 0) {
    string end = trim(expr.substr(pold, p-pold));
    if (end.length() > 0)
      statements.push_back(end);
  }
}

ParseNode *Parser::parseif (const string &expr, size_t &pos) {
  eatWhite(expr, pos);
  if (expr[pos] != '(')
    throw meosException("Expected ( after if,\nX#" + expr);
  size_t start = pos + 1;
  matchSE(expr, pos, '(', ')');

  IfNode *ifn = getif ();
  ifn->condition = parseStatement(expr.substr(start, pos - start - 1), false);
  eatWhite(expr, pos);

  ifn->iftrue = parseStatement(expr.substr(pos), true);

  return ifn;
}

ParseNode *Parser::parseReturn(const string &expr, size_t &pos) {
  UnaryOperatorNode *un = getUnary();
  un->right = parseStatement(expr.substr(pos), false);
  un->op = OpReturn;
  return un;
}

ParseNode *Parser::parseWhile(const string &expr, size_t &pos) {
  eatWhite(expr, pos);
  if (expr[pos] != '(')
    throw meosException("Expected ( after while,\nX#" + expr);
  size_t start = pos + 1;
  matchSE(expr, pos, '(', ')');

  WhileNode *wh = getWhile();
  wh->condition = parseStatement(expr.substr(start, pos - start - 1), false);
  eatWhite(expr, pos);

  wh->body = parseStatement(expr.substr(pos), true);

  return wh;
}

ParseNode *Parser::parseFor(const string &expr, size_t &pos) {
  eatWhite(expr, pos);
  if (expr[pos] != '(')
    throw meosException("Expected ( after while,\nX#" + expr);
  size_t start = pos + 1;
  matchSE(expr, pos, '(', ')');
  map<char,char> se;
  vector<string> parts;
  splitStatements(expr.substr(start, pos - start - 1), ';', se, parts);

  if (parts.size() != 3)
    throw meosException("Syntax error in for(.;.;.),\nX#" + expr);

  ForNode *fn = getFor();
  if (!trim(parts[0]).empty())
    fn->start =  parseStatement(parts[0], false);
  else
    fn->start = getStatement();

  fn->condition = parseStatement(parts[1], false);

  if (!trim(parts[2]).empty())
    fn->update = parseStatement(parts[2], false);
  else
    fn->update = getStatement();

  eatWhite(expr, pos);

  fn->body = parseStatement(expr.substr(pos), true);
  return fn;
}

int Parser::getLevel(Operator op) {
  switch (op) {
  case OpNone:
    return -1;
  case OpPlus:
  case OpMinus:
    return 2;
  case  OpTimes:
  case  OpDivide:
  case  OpMod:
    return 3;

  case  OpMax:
  case  OpMin:
  case OpInc:
  case OpDec:
    return 4;

  case  OpEquals:
  case  OpNotEquals:
  case  OpLess:
  case  OpMore:
  case  OpLessEquals:
  case  OpMoreEquals:
    return 1;

  case OpAnd:
  case OpOr:
    return 0;

  case OpAssign:
    return 0;
  }

  return -1;
}

string Parser::parseMethod(const string &expr, size_t &pos) {
  pos++;
  string sword = readWord(expr, pos);
  eatWhite(expr, pos);
  if (expr[pos] != '(') 
    throw meosException("Expected ( after X#" + sword);
  size_t start = pos + 1;
  matchSE(expr, pos, '(', ')');
  if (trim(expr.substr(start, pos-start-1)).size() > 0)
    throw meosException("No arguments expected for X()#" + sword);
  
  return sword;
}

ParseNode *Parser::parseValue(const string &word, const string &expr, size_t &pos) {
  eatWhite(expr, pos);
  if (expr[pos] == '[' || expr[pos] == '.') {
    if (isdigit(word[0]))
      throw meosException("Invalid operator in litteral,\nX#" + expr);

    if (expr[pos] == '[') {
      size_t start = pos + 1;
      matchSE(expr, pos, '[', ']');
      ArrayValueNode *arr = getArrayValue();
      arr->expr = word;
      arr->index = parseStatement(expr.substr(start, pos-start-1), false);
      
      eatWhite(expr, pos);
      if (expr[pos] == '[') {
        start = pos + 1;
        matchSE(expr, pos, '[', ']');
        arr->index2 = arr->index;
        arr->index = parseStatement(expr.substr(start, pos-start-1), false);
      }
      else if (expr[pos] == '.') {
        string sword = parseMethod(expr, pos);
        if (sword != "size") {
          throw meosException("Unknown method X#" + word + "." + sword);
        }
        UnaryOperatorNode *un = getUnary();
        un->op = OpSizeSub;
        un->right = arr;
        return un;
      }

      return arr;
    }
    else if (expr[pos] == '.') {
      string sword = parseMethod(expr, pos);
      UnaryOperatorNode *un = getUnary();
      un->op = OpNone;

      if (sword == "size") {
        if (isMatrix(word))
          un->op = OpSizeBase;
        else  
          un->op = OpSize;
      }
      else if (sword == "sort") {
        un->op = OpSortArray;
      }

      if (un->op != OpNone) {
        ValueNode *vn = getValue();
        vn->value = word;
        un->right = vn;
        return un;
      }
      else
        throw meosException("Unknown method X#" + word + "." + sword);
    }
  }
  ValueNode *vn = getValue();
  vn->value = word;
  return vn;
}


ParseNode *Parser::parseFunction(const string &name, const string &expr, size_t &pos) {
  Operator op = OpNone;

  if (name == "if" || name == "for" || name == "while" || name == "break" || name == "return")
    throw meosException("Unexpected " + name + ",\n#" + expr);

  if (name == "max")
    op = OpMax;
  else if (name == "min")
    op = OpMin;

  if (op == OpNone)
    return 0;

  eatWhite(expr, pos);

  if (expr[pos] != '(')
    throw meosException("Expected (,\nX#" + expr);

  size_t start = pos + 1;
  matchSE(expr, pos, '(', ')');

  //splitStatements
  map<char, char> se;
  se['('] = ')';
  vector<string> arg;
  splitStatements(expr.substr(start, pos-start-1), ',', se, arg);

  if (arg.size() != 2)
    throw meosException("Expected 2 arguments, got X,\nY#"+itos(arg.size()) + "#" + expr);

  BinaryOperatorNode *bin = getBinary();
  bin->left = parseStatement(arg[0], false);
  bin->right = parseStatement(arg[1], false);
  bin->op = op;

  return bin;
}


Parser::Operator Parser::parseOperator(const string &expr, size_t &pos, int level) {
  size_t npos = pos;
  Operator op = parseOperatorAux(expr, npos);

  if (op == OpNone || getLevel(op) != level)
    return OpNone;
  pos = npos;
  return op;
}

void Parser::eatWhite(const string &expr, size_t &pos) {
  while (pos < expr.size() && isspace(expr.at(pos))) {
    pos++;
  }
}

Parser::Operator Parser::parseOperatorAux(const string &expr, size_t &pos) {
  eatWhite(expr, pos);

  size_t pos2 = pos;
  string opWord = readWord(expr, pos2);
  if (opWord == "or") {
    pos = pos2;
    return OpOr;
  }
  else if (opWord == "and") {
    pos = pos2;
    return OpAnd;
  }

  if (pos < expr.size()) {
    char p1 = 0, p2 = 0;
    p1 = expr[pos++];
    if (pos < expr.size() && p1 != '(' && p1 != ')' ) {
      p2 = expr[pos++];
      if (p2 == 0 || isalnum(p2) || isspace(p2) || (p2 == '-' && p1 != '-') ||
          p2 == '!' || (p2 == '+' && p1 != '+') || p2=='(') {
        pos--;
        p2 = 0;
      }
    }

    if (p1 == '(')
      return OpLeftP;
    else if (p1 == ')')
      return OpRightP;
    else if (p1 == '=' && p2 == '=')
      return OpEquals;
    else if (p1 == '=' && p2 == 0)
      return OpAssign;
    else if (p1 == '!' && p2 == '=')
      return OpNotEquals;
    else if (p1 == '<' && p2 == '=')
      return OpLessEquals;
    else if (p1 == '>' && p2 == '=')
      return OpMoreEquals;
    else if (p1 == '<' && p2 == 0)
      return OpLess;
    else if (p1 == '>' && p2 == 0)
      return OpMore;
    else if (p1 == '+' && p2 == 0)
      return OpPlus;
    else if (p1 == '-' && p2 == 0)
      return OpMinus;
    else if (p1 == '*' && p2 == 0)
      return OpTimes;
    else if (p1 == '/' && p2 == 0)
      return OpDivide;
    else if (p1 == '%' && p2 == 0)
      return OpMod;
    else if (p1 == '&' && p2 == '&')
      return OpAnd;
    else if (p1 == '|' && p2 == '|')
      return OpOr;
    else if (p1 == '!' && p2 == 0)
      return OpNot;
    else if (p1 == '+' && p2 == '+')
      return OpInc;
    else if (p1 == '-' && p2 == '-')
      return OpDec;

    string op = expr.substr(0, 10);
    throw meosException("Invalid operator X#" + op);
  }
  return OpNone;
}

// 3*2 + 4*4 == 5

ParseNode *Parser::parseStatement(ParseNode *left, const string &expr, 
                                  size_t &pos, int level, bool reverseSign) {
  eatWhite(expr, pos);
  if (pos == expr.length())
    return left;
  Operator op = parseOperator(expr, pos, level);
  while (op == OpNone) {
    if (level <= levelMax) {
      ParseNode *newLeft = parseStatement(left, expr, pos, level + 1, reverseSign);
      if (newLeft == left)
        return left;
      else
        left = newLeft;
    }
    else
      return left;

    op = parseOperator(expr, pos, level);
  }

  if (op == OpInc || op == OpDec) {
    UnaryOperatorNode *un = getUnary();
    un->op = op == OpInc ? OpIncPost : OpDecPost;
    un->right = left;
    return un;
  }

  if (reverseSign) {
    if (op == OpMinus) {
      op = OpPlus;
    }
    else if (op == OpPlus) {
      op = OpMinus;
      reverseSign = false;
    }
  }
  else if (op == OpMinus) {
    reverseSign = true;
  }

  if (op == OpAssign && !left->isVariable()) {
    throw meosException("Invalid assignment X#" + expr);
  }

  BinaryOperatorNode *bin = getBinary();
  bin->left = left;
  bin->op = op;

  eatWhite(expr, pos);

  if (op == OpAssign) {
    bin->right = parseStatement(expr.substr(pos),false);
    return bin;
  }
  /*else if (expr[pos] == '(') {
    size_t start = pos + 1;
    matchSE(expr, pos, '(', ')');
    string subExpr = expr.substr(start, pos-start-1);
    ParseNode *pn = parseStatement(subExpr, false);
    bin->right = parseStatement(pn, expr, pos, level);
    
    //bin->right = parseStatement(subExpr, false);
    return bin;
  }*/
  else {
    size_t pos2 = pos;
    Operator op2 = OpNone;
    if (!isalnum(expr[pos])) {
      op2 = parseOperatorAux(expr, pos2);
      if (op2 == OpMinus || op2 == OpPlus || op2 == OpNot) {
        pos = pos2;
      }
      else if (op2 == OpInc) {
        pos = pos2;
        op2 = OpIncPre;
      }
      else if (op2 == OpDec) {
        pos = pos2;
        op2 = OpDecPre;
      }
      else if (op2 == OpLeftP) {
        op2 = OpNone;
      }
      else if (op2 != OpNone) {
        throw meosException("Syntax error,\nX#" + expr);
      }
      eatWhite(expr, pos);
    }

    if (expr[pos] == '(') {
      size_t start = pos + 1;
      matchSE(expr, pos, '(', ')');
      string subExpr = expr.substr(start, pos-start-1);
      ParseNode *pn = parseStatement(subExpr, false);
      
      if (op2 != 0) {
        UnaryOperatorNode *un = getUnary();
        un->op = op2;
        un->right = pn;
        pn = un;
      }

      bin->right = parseStatement(pn, expr, pos, level, false);
    
      //bin->right = parseStatement(subExpr, false);
      return bin;
    }
    else if (isalnum(expr[pos])) {
      string word = readWord(expr, pos);
      ParseNode *right;
      ParseNode *pn = parseFunction(word, expr, pos);
      if (pn != 0) {
        if (op2 != 0) {
          UnaryOperatorNode *un = getUnary();
          un->op = op2;
          un->right = pn;
          pn = un;
        }
        right = parseStatement(pn, expr, pos, level, reverseSign);
      }
      else if (!word.empty()) {
        ParseNode *vn = parseValue(word, expr, pos);
        //ValueNode *vn = getValue();
        //vn->value = word;
        pn = vn;
        if (op2 != 0) {
          UnaryOperatorNode *un = getUnary();
          un->op = op2;
          un->right = pn;
          pn = un;
        }
        right = parseStatement(pn, expr, pos, level, reverseSign);
      }
      else {
        throw meosException("Unexpected ending operator X#" + expr);
      }

      bin->right = right;
      return bin;
    }
  }
  throw meosException("Syntax error,\nX#" + expr);
}

ParseNode *Parser::parseStatement(const string &expr, bool primary) {
  size_t pos = 0;
  eatWhite(expr, pos);
  ParseNode *left = 0;
  if (expr[pos] == '(') {
    size_t start = pos + 1;
    matchSE(expr, pos, '(', ')');
    string subExpr = expr.substr(start, pos-start-1);
    left = parseStatement(subExpr, false);
  }
  else if (expr[pos] == '{') {
    if (!primary)
      throw meosException("Unexpeccted {,\nX" + expr);
    size_t start = pos + 1;
    matchSE(expr, pos, '{', '}');
    string subExpr = expr.substr(start, pos-start-1);
    return parse(subExpr);
  }
  else {
    size_t pos2 = pos;
    Operator op2 = OpNone;
    if (!isalnum(expr[pos])) {
      op2 = parseOperatorAux(expr, pos2);
      if (op2 == OpMinus || op2 == OpPlus || op2 == OpNot) {
        pos = pos2;
      }
      else if (op2 == OpInc) {
        pos = pos2;
        op2 = OpIncPre;
      }
      else if (op2 == OpDec) {
        pos = pos2;
        op2 = OpDecPre;
      }
      else if (op2 != OpNone) {
        throw meosException("Syntax error,\nX#" + expr);
      }
      primary = false;
    }

    eatWhite(expr, pos);
    if (op2 != OpNone && expr[pos] == '(') {
      size_t start = pos + 1;
      matchSE(expr, pos, '(', ')');
      string subExpr = expr.substr(start, pos-start-1);
      UnaryOperatorNode *un = getUnary();
      un->op = op2;
      un->right = parseStatement(subExpr, false);
      left = un;
    }
    else if (isalnum(expr[pos])) {
      string word = readWord(expr, pos);
      if (word == "if") {
        if (!primary)
          throw meosException("Unexpected if\nX#" + expr);
        return parseif (expr, pos);
      }
      else if (word == "return") {
        if (!primary)
          throw meosException("Unexpected return\nX#" + expr);
        return parseReturn(expr, pos);
      }
      else if (word == "while") {
        if (!primary)
          throw meosException("Unexpected while\nX#" + expr);
        return parseWhile(expr, pos);
      }
      else if (word == "for") {
        if (!primary)
          throw meosException("Unexpected for\nX#" + expr);
        return parseFor(expr, pos);
      }
      else if (word == "break") {
        if (!primary)
          throw meosException("Unexpected break\nX#" + expr);
        UnaryOperatorNode *un = getUnary();
        un->op = OpBreak;
        return un;
      }

      left = parseFunction(word, expr, pos);

      if (left == 0) {
        //ValueNode *vn = getValue();
        //vn->value = word;
        left = parseValue(word, expr, pos);
      }

      if (op2 != OpNone) {
        UnaryOperatorNode *un = getUnary();
        un->op = op2;
        un->right = left;
        left = un;
      }
    }
    else
      throw meosException("Syntax error,\nX#" + expr);
  }
  ParseNode *node = parseStatement(left, expr, pos, 0, false);

  if (node == 0)
    throw meosException("Syntax error,\nX#" + expr);

  return node;
}

ParseNode *Parser::parse(const string &expr) {
  vector<string> lines;
  split(expr, "\n", lines);
  string exprNoComments;
  exprNoComments.reserve(expr.size()+4);
  for (size_t k = 0; k < lines.size(); k++) {
    int ix = lines[k].find("//");
    if (ix != lines[k].npos)
      exprNoComments.append(lines[k].substr(0, ix));
    else
      exprNoComments.append(lines[k]);

    exprNoComments.append("\n");
  }

  vector<string> spv;
  map<char, char> se;
  se['{'] = '}';
  splitStatements(exprNoComments, ';', se, spv);
  /*for (size_t k = 0; k < spv.size(); k++) {
    int ix = spv[k].find_first_of("//");
    if (ix != spv[k].npos)
      spv[k] = trim(spv[k].substr(0, ix));
  }*/

  if (spv.size() == 1)
    return parseStatement(spv[0], true);
  else {
    StatementNode *sn = getStatement();
    ParseNode *ret = sn;
    StatementNode *oldSN = 0;
    for (size_t k = 0; k < spv.size(); k++) {
      size_t pos = 0;
      eatWhite(spv[k], pos);
      if (readWord(spv[k], pos) == "else") {
        if (oldSN != 0 && typeid(*(oldSN->node))==typeid(IfNode)) {
          oldSN->next = 0;
          sn = oldSN;
          IfNode *ifn = dynamic_cast<IfNode *>(sn->node);
          while (ifn != 0 && ifn->iffalse) {
            ifn = dynamic_cast<IfNode *>(ifn->iffalse);
          }
          if (ifn) {
            oldSN = 0;
            ifn->iffalse = parseStatement(spv[k].substr(pos), true);
          }
          else
            throw meosException("else without matching if,\nX#" + spv[k]);
        }
        else
          throw meosException("else without matching if,\nX#" + spv[k]);
      }
      else
        sn->node = parseStatement(spv[k], true);

      if (k+1 < spv.size())
        sn->next = getStatement();
      oldSN = sn;
      sn = sn->next;
    }
    return ret;
  }
}

int Parser::evaluate(const string &input) const {
  if (input.empty())
    throw meosException("Empty expression");

  if (isdigit(input[0]))
    return atoi(input.c_str());

  {
    map<string, Symbol>::const_iterator res = symb.find(input);
    if (res != symb.end()) {
      if (res->second.value.empty())
        throw meosException("Internal error");
      if (res->second.value[0].size() == 1)
        return res->second.value[0][0];
      throw meosException("X is an array.#" + input);
    }
  }

  map<string, vector<int> >::iterator res = var.find(input);
  if (res != var.end()) {
    if (res->second.size() == 1)
      return res->second[0];
    throw meosException("X is an array.#" + input);
  }
  throw meosException("Unknown symbol X#" + input);
}

int Parser::evaluate(const string &input, int index, int index2) const {
  if (input.empty())
    throw meosException("Empty expression");

  if (isdigit(input[0]))
    return atoi(input.c_str());

  map<string, Symbol>::const_iterator res = symb.find(input);
  if (res != symb.end()) {
    if (size_t(index2) < res->second.value.size() && 
        size_t(index) < res->second.value[index2].size())
      return res->second.value[index2][index];
    if (index2 == 0)
      throw meosException("Index X in Y is out of range.#" + itos(index) + "#" + input);
    else
      throw meosException("Index X in Y is out of range.#" + itos(index2) + "," + itos(index) + "#" + input);
  }

  map<string, vector<int> >::const_iterator res2 = var.find(input);
  if (res2 != var.end()) {
    if (index2 != 0)
      throw meosException("Index X in Y is out of range.#" + itos(index2) + "#" + input);
    if (size_t(index) < res2->second.size())
      return res2->second[index];
    throw meosException("Index X in Y is out of range.#" + itos(index) + "#" + input);
  }
  throw meosException("Unknown symbol X#" + input);
}

int Parser::evaluateSize(const string &input, int index) const {
  if (input.empty())
    throw meosException("Empty expression");

  if (isdigit(input[0]))
    throw meosException("Constant expression");
  {
    map<string, Symbol >::const_iterator res = symb.find(input);

    if (res != symb.end()) {
      if (index == -1) 
        return res->second.value.size();
      else {
        if (size_t(index) < res->second.value.size())
          return res->second.value[index].size();
        else
          throw meosException("Index out of range for X.#" + input);
      }
    }
  }

  map<string, vector<int> >::const_iterator res = var.find(input);
  if (res != var.end()) {
    if (index != 0)
      throw meosException("Index out of range for X.#" + input);
    return res->second.size();
  }
  throw meosException("Unknown symbol X#" + input);
}

void Parser::sortArray(const string &input) const {
  map<string, vector<int> >::iterator res = var.find(input);
  if (res != var.end()) {
    sort(res->second.begin(), res->second.end());
    return;
  }
  throw meosException("Unknown symbol X#" + input);
}

void Parser::storeVariable(const string &input, const vector<int> &value) const {  
  if (symb.count(input))
    throw meosException("Duplicate symbol X#" + input);
  var[input] = value;
}

void Parser::storeVariable(const string &input, int value) const {
  if (symb.count(input))
    throw meosException("Duplicate symbol X#" + input);
  vector<int> &iv = var[input];
  iv.resize(1);
  iv[0] = value;
}

void Parser::storeVariable(const string &input, int index, int value) const {
  if (symb.count(input))
    throw meosException("Duplicate symbol X#" + input);
  if (index < 0 || index>1024)
    throw meosException("Index out of range for X.#" + input);

  vector<int> &iv = var[input];
  if (iv.size() <= size_t(index))
    iv.resize(index+1);
  iv[index] = value;
}

Parser::Parser() {

}

Parser::~Parser() {
  clear();
}

void Parser::clear() {
  for (size_t k = 0; k < nodes.size(); k++) {
    delete nodes[k];
    nodes[k] = 0;
  }
  nodes.clear();
}

Parser::UnaryOperatorNode *Parser::getUnary() {
  nodes.push_back(new UnaryOperatorNode());
  return (UnaryOperatorNode *)nodes.back();
}

Parser::BinaryOperatorNode *Parser::getBinary() {
  nodes.push_back(new BinaryOperatorNode());
  return (BinaryOperatorNode *)nodes.back();
}

Parser::ValueNode *Parser::getValue() {
  nodes.push_back(new ValueNode());
  return (ValueNode *)nodes.back();
}

Parser::ArrayValueNode *Parser::getArrayValue() {
  nodes.push_back(new ArrayValueNode());
  return (ArrayValueNode *)nodes.back();
}

Parser::StatementNode *Parser::getStatement() {
  nodes.push_back(new StatementNode());
  return (StatementNode *)nodes.back();
}

Parser::IfNode *Parser::getif () {
  nodes.push_back(new IfNode());
  return (IfNode *)nodes.back();
}

Parser::WhileNode *Parser::getWhile() {
  nodes.push_back(new WhileNode());
  return (WhileNode *)nodes.back();
}

Parser::ForNode *Parser::getFor() {
  nodes.push_back(new ForNode());
  return (ForNode *)nodes.back();
}

ParseNode::~ParseNode() {
}


Parser::StatementNode::StatementNode() {
  node = 0;
  next = 0;
}

Parser::StatementNode::~StatementNode() {
  node = 0;
  next = 0;
}

int Parser::StatementNode::evaluate(const Parser &parser) const {
  if (node == 0)
    throw meosException("Nullpointer");
  parser.returnMode = false;
  parser.breakMode = 0;
  const StatementNode *c = this;
  int ret = 0;
  while(c && !parser.returnMode) {
    parser.ignoreValue = false;
    int val = c->node->evaluate(parser);
    if (!parser.ignoreValue)
      ret = val;
    if (parser.breakMode>0) {
      break;
    }
    c = c->next;
  }
  return ret;
}

Parser::ValueNode::ValueNode() {
}
Parser::ValueNode::~ValueNode() {
}

int Parser::ValueNode::evaluate(const Parser &parser) const {
  return parser.evaluate(value);
}
bool Parser::ValueNode::isVariable() const {
  return value.length()>0 && isalpha(value[0]);
}

Parser::BinaryOperatorNode::BinaryOperatorNode() {
  left = 0;
  right = 0;
}

Parser::BinaryOperatorNode::~BinaryOperatorNode() {
  left = 0;
  right = 0;
}

int Parser::BinaryOperatorNode::evaluate(const Parser &parser) const {
  if (left == 0 || right == 0)
    throw meosException("Internal error");

  switch (op) {
  case OpPlus:
    return left->evaluate(parser) + right->evaluate(parser);
  case OpMinus:
    return left->evaluate(parser) - right->evaluate(parser);
  case OpTimes:
    return left->evaluate(parser) * right->evaluate(parser);
  case OpDivide:
    return left->evaluate(parser) / right->evaluate(parser);
  case OpMod:
    return left->evaluate(parser) % right->evaluate(parser);

  case OpEquals:
    return left->evaluate(parser) == right->evaluate(parser);
  case OpNotEquals:
    return left->evaluate(parser) != right->evaluate(parser);
  case OpLess:
    return left->evaluate(parser) < right->evaluate(parser);
  case OpLessEquals:
    return left->evaluate(parser) <= right->evaluate(parser);
  case OpMore:
    return left->evaluate(parser) > right->evaluate(parser);
  case OpMoreEquals:
    return left->evaluate(parser) >= right->evaluate(parser);

  case OpOr:
    return left->evaluate(parser) || right->evaluate(parser);
  case OpAnd:
    return left->evaluate(parser) && right->evaluate(parser);

  case OpMax:
    return max(left->evaluate(parser), right->evaluate(parser));
  case OpMin:
    return min(left->evaluate(parser), right->evaluate(parser));

  case OpAssign: {
    ValueNode *vn = dynamic_cast<ValueNode *>(right);
    if (vn != 0) {
      if (parser.isMatrix(vn->value))
        throw meosException("Cannot assign matrix X#"+vn->value);
      if (parser.isVector(vn->value)) {
        left->assignVector(parser, parser.getVector(vn->value, 0));
        parser.ignoreValue = true;
        return -1;
      }
    }
    ArrayValueNode *avn = dynamic_cast<ArrayValueNode *>(right);
    if (avn != 0 && parser.isMatrix(avn->expr) && avn->index2 == 0) {
      left->assignVector(parser, parser.getVector(avn->expr, avn->index->evaluate(parser)));
      parser.ignoreValue = true;
      return -1;
    }

    int val = right->evaluate(parser);
    left->assign(parser, val);
    return val;
  }

  }

  throw meosException("Internal error, unknown operator");
}


Parser::UnaryOperatorNode::UnaryOperatorNode() {
  right = 0;
}

Parser::UnaryOperatorNode::~UnaryOperatorNode() {
  right = 0;
}

int Parser::UnaryOperatorNode::evaluate(const Parser &parser) const {
  if (op == OpBreak) {
    parser.ignoreValue = true;
    parser.breakMode = 1;
    return -1;
  }

  if (right == 0)
    throw meosException("Internal error");

  switch (op) {
    case OpReturn: {
      int val = right->evaluate(parser);
      parser.returnMode = true;
      return val;
    }
    case OpMinus:
      return -right->evaluate(parser);
    case OpNot: {
      return right->evaluate(parser) == 0;
    }
    case OpPlus:
      return right->evaluate(parser);
    case OpIncPost: {
      int val = right->evaluate(parser);
      right->assign(parser, val + 1);
      return val;
    }
    case OpDecPost: {
      int val = right->evaluate(parser);
      right->assign(parser, val-1);
      return val;
    }
    case OpIncPre: {
      int val = right->evaluate(parser) + 1;
      right->assign(parser, val);
      return val;
    }
    case OpDecPre: {
      int val = right->evaluate(parser) - 1;
      right->assign(parser, val);
      return val;
    }
    case OpSize: {
       ValueNode &vn= dynamic_cast<ValueNode &>(*right);
       return parser.evaluateSize(vn.value, 0);
    }
    case OpSizeBase: {
       ValueNode &vn= dynamic_cast<ValueNode &>(*right);
       return parser.evaluateSize(vn.value, -1);
    }
    case OpSizeSub: {
      ArrayValueNode &vn= dynamic_cast<ArrayValueNode &>(*right);
      return parser.evaluateSize(vn.expr, vn.index->evaluate(parser));
    }
    case OpSortArray: {
       ValueNode &vn= dynamic_cast<ValueNode &>(*right);
       parser.sortArray(vn.value);
       parser.ignoreValue = true;
       return -1;
    }
  }

  throw meosException("Internal error, unknown operator");
}

Parser::IfNode::IfNode() {
  condition = 0;
  iftrue = 0;
  iffalse = 0;
}

Parser::IfNode::~IfNode() {
  condition = 0;
  iftrue = 0;
  iffalse = 0;
}

int Parser::IfNode::evaluate(const Parser &parser) const {
  if (condition->evaluate(parser) != 0) {
    return iftrue->evaluate(parser);
  }
  if (iffalse)
    return iffalse->evaluate(parser);

  parser.ignoreValue = true;
  return -1;
}

Parser::WhileNode::WhileNode() {
  condition = 0;
  body = 0;
}

Parser::WhileNode::~WhileNode() {
  condition = 0;
  body = 0;
}

int Parser::WhileNode::evaluate(const Parser &parser) const {
 if (condition == 0 || body == 0)
    throw meosException("Internal error in while");
  int maxCount = 1000;
  int ret = -1;
  bool used = false;
  while (condition->evaluate(parser) != 0 && !parser.returnMode && --maxCount > 0) {
    ret = body->evaluate(parser);
    if (parser.breakMode>0) {
      parser.breakMode--;
      break;
    }
    used = true;
  }
  if (maxCount <= 0) {
    throw meosException("Stalled while loop");
  }

  if (!used)
    parser.ignoreValue = true;
  return ret;
}


Parser::ForNode::ForNode() {
  condition = 0;
  start = 0;
  update = 0;
  body = 0;
}

Parser::ForNode::~ForNode() {
  condition = 0;
  start = 0;
  update = 0;
  body = 0;
}

int Parser::ForNode::evaluate(const Parser &parser) const {
  if (start == 0 || condition == 0 || update == 0 || body == 0)
    throw meosException("Internal error in for");
  int maxCount = 1000;
  int ret = -1;
  bool used = false;
  for (start->evaluate(parser);
       condition->evaluate(parser) != 0 && !parser.returnMode && --maxCount > 0;
       update->evaluate(parser) ) {
    ret = body->evaluate(parser);
    used = true;
  }
  if (maxCount <= 0) {
    throw meosException("Stalled for loop");
  }

  if (!used)
    parser.ignoreValue = true;
  return ret;
}

Parser::ArrayValueNode::ArrayValueNode() {
  index = 0;
  index2 = 0;
}
Parser::ArrayValueNode::~ArrayValueNode() {
  index = 0;
  index2 = 0;
}

int Parser::ArrayValueNode::evaluate(const Parser &parser) const {
  if (index2 == 0)
    return parser.evaluate(expr, index->evaluate(parser), 0);
  else
    return parser.evaluate(expr, index->evaluate(parser), index2->evaluate(parser));
}

bool Parser::ArrayValueNode::isVariable() const {
  return true;
}

void ParseNode::assign(const Parser &parser, int value) const {
  throw meosException("Illegal assignment");
}

void ParseNode::assignVector(const Parser &parser, const vector<int> &value) const {
  throw meosException("Illegal assignment");
}
 
void Parser::ValueNode::assign(const Parser &parser, int in_value) const {
  parser.storeVariable(value, in_value);
}

void Parser::ValueNode::assignVector(const Parser &parser, const vector<int> &in_value) const {
  parser.storeVariable(value, in_value);
}


void Parser::ArrayValueNode::assign(const Parser &parser, int value) const {
  int ix2;
  if (index2 != 0 && (ix2 = index2->evaluate(parser)) != 0)
      throw meosException("Index X in Y is out of range.#" + itos(ix2) + "#" + expr); 
  parser.storeVariable(expr, index->evaluate(parser), value);
}

void Parser::ArrayValueNode::assignVector(const Parser &parser, const vector<int> &in_value) const {
  if (in_value.size() != 1)
    throw meosException("Vector cannot be assigned to X[i]#" + expr);
  parser.storeVariable(expr, index->evaluate(parser), in_value[0]);
}

void Parser::declareSymbol(const char *name, const string &desc, bool isVector, bool isMatrix) {
  assert(symb.count(name) == 0 || (symb[name].isVector == isVector && symb[name].isMatrix == isMatrix));
  symb[name].desc = desc;
  symb[name].isVector = isVector;
  symb[name].isMatrix = isMatrix;
}

bool Parser::isMatrix(const string &input) const {
  map<string, Symbol>::const_iterator res = symb.find(input);
  return res != symb.end() && res->second.isMatrix;
}

bool Parser::isVector(const string &input) const {
  map<string, Symbol>::const_iterator res = symb.find(input);
  if (res != symb.end()) 
    return !res->second.isMatrix && res->second.value[0].size() != 1;

  map<string, vector<int> >::const_iterator resv = var.find(input);
  return resv != var.end() && resv->second.size() != 1;
}

const vector<int> &Parser::getVector(const string &symbol, int index) const{
  map<string, Symbol>::const_iterator res = symb.find(symbol);
  if (res != symb.end()) {
    if (size_t(index) < res->second.value.size())
      return res->second.value[index];
    else
      throw meosException("Index out of range for X.#" + symbol);
  }
  map<string, vector<int> >::const_iterator resv = var.find(symbol);
  if (resv != var.end())
    return resv->second;

  throw meosException("Unknown symbol X#" + symbol);
}

void Parser::addSymbol(const char *name, const string &value) {
  assert(symb.count(name) && !symb[name].isVector);
  symb[name].value.resize(1);
  vector<int> &v = symb[name].value[0];
  v.resize(1);
  v[0] = atoi(value.c_str());
}

void Parser::addSymbol(const char *name, int value) {
  assert(symb.count(name) && !symb[name].isVector);
  symb[name].value.resize(1);
  vector<int> &v = symb[name].value[0];
  v.resize(1);
  v[0] = value;
}

void Parser::addSymbol(const char *name, const vector<string> &value) {
  assert(symb.count(name) && symb[name].isVector);
  symb[name].value.resize(1);
  vector<int> &v = symb[name].value[0];
  v.resize(value.size());
  for (size_t k = 0; k < value.size(); k++)
    v[k] = atoi(value[k].c_str());
}

void Parser::addSymbol(const char *name, const vector<int> &value) {
  assert(symb.count(name) && symb[name].isVector);
  symb[name].value.resize(1);
  symb[name].value[0] = value;
}

void Parser::addSymbol(const char *name, vector< vector<int> > &value) {
  assert(symb.count(name) && symb[name].isVector);
  symb[name].value.swap(value);
}

void Parser::removeSymbol(const char *name) {
  symb[name].value.clear();
}

void Parser::clearSymbols() {
  symb.clear();
}

void Parser::clearVariables() const {
  var.clear();
}

void Parser::takeVariable(const char*name, vector<int> &val) const {
  map<string, vector<int> >::iterator resv = var.find(name);
  if (resv != var.end()) {
    vector<int> &res = resv->second;
    val.swap(res);
  }
  else
    val.clear();
}

void Parser::getSymbols(vector< pair<wstring, size_t> > &symbOut) const {
  int iter = 0;
  for(map<string, Symbol>::const_iterator it = symb.begin(); it != symb.end(); ++it) {
    if (it->second.isMatrix)
      symbOut.push_back(make_pair(gdi_main->widen(it->first) + L"[][]\t" + lang.tl(it->second.desc), iter++));
    else if (it->second.isVector)
      symbOut.push_back(make_pair(gdi_main->widen(it->first) + L"[]\t" + lang.tl(it->second.desc), iter++));
    else
      symbOut.push_back(make_pair(gdi_main->widen(it->first) + L"\t" + lang.tl(it->second.desc), iter++));
  }
}

void Parser::getSymbolInfo(int ix, wstring &name, wstring &desc) const {
  int iter = 0;
  for(map<string, Symbol>::const_iterator it = symb.begin(); it != symb.end(); ++it) {
    if (ix == iter++) {
      if (it->second.isMatrix)
        name = gdi_main->widen(it->first) + L"[][]";
      else if (it->second.isVector)
        name = gdi_main->widen(it->first) + L"[]";
      else
        name = gdi_main->widen(it->first);
      desc = gdi_main->widen(it->second.desc);

      return;
    }
  }
  throw meosException("Internal error");
}


static void assertEq(int a, int b) {
  if (a != b) {
    string s = "Expected X, was Y#" + itos(a) + "#" + itos(b);
    throw s;
  }
}

void Parser::test() {
  Parser parser;
  vector<int> tt;
  tt.push_back(1);
  tt.push_back(4);
  tt.push_back(9);
  parser.declareSymbol("tt", "", true);
  parser.declareSymbol("tt2", "", true);
  parser.declareSymbol("ttt", "", true, true);
  parser.declareSymbol("t", "", false);

  vector<int> tt2;
  
  tt2.push_back(3);
  tt2.push_back(4);
  tt2.push_back(2);
  tt2.push_back(1);

  parser.addSymbol("tt", tt);
  parser.addSymbol("tt2", tt2);
  parser.addSymbol("t", 3);
  vector< vector<int> > ttt;
  ttt.push_back(tt);
  ttt.push_back(tt);
  ttt.back().push_back(55);
  parser.addSymbol("ttt", ttt);
  
  ParseNode *pn;
  pn = parser.parse("a*b*e + c*d + f == 5*4 + 3*2; if (foo) {aa; {x;y} } rolf2=nasse; rolf2 ");

  pn = parser.parse("{g=1;}{h=1} {(1)} {{(h++) }} return g+h");
  assertEq(pn->evaluate(parser), 3);

  pn = parser.parse("16-8+4-2+1"); // 16-(8-4+2-1)
                                   // 16-(8-(4-2+1))
                                   // 16-(8-(4-(2-1))
  assertEq(pn->evaluate(parser), 11);

  pn = parser.parse("16-8-4-2-1)");
  assertEq(pn->evaluate(parser), 1);

  pn = parser.parse("16-8+4-2-1)");
  assertEq(pn->evaluate(parser), 9);

  pn = parser.parse("16-(8+4-2+1)");
  assertEq(pn->evaluate(parser), 5);

  pn = parser.parse("16-2*4*1+2*2*1-2*1");
  assertEq(pn->evaluate(parser), 10);

  pn = parser.parse("a = (1+2) * (3+4);b = a-2*10");
  assertEq(pn->evaluate(parser), 1);

  pn = parser.parse("1 + 2*2"); 
  assertEq(pn->evaluate(parser), 5);
  
  pn = parser.parse("1 + (1+1)*2"); 
  assertEq(pn->evaluate(parser), 5);
  
  pn = parser.parse("1 * -(1+1)*+(1+1)+10"); 
  assertEq(pn->evaluate(parser), 6);

  pn = parser.parse("3*5 + (3-1)*3*2 - 5*3"); 
  assertEq(pn->evaluate(parser), 12);

  pn = parser.parse("{a = 1; {b=2;}} if (t == a+1*b) {a++; if (a>1) {a = a*2}; return a+6;} else a--; +1+1");
  assertEq(pn->evaluate(parser), 10);

  pn = parser.parse("a = 3; if (1+1 == a) return 2; return 5*5*5-20;");
  assertEq(pn->evaluate(parser), 105);

  pn = parser.parse("max(1,2);");
  assertEq(pn->evaluate(parser), 2);

  pn = parser.parse("max(1,2) + min(max(0,1),2)");
  assertEq(pn->evaluate(parser), 3);

  pn = parser.parse("-1*-1*2+-3*3+-max(-1, 2)");
  assertEq(pn->evaluate(parser), -9);

  pn = parser.parse("1+-1+1");
  assertEq(pn->evaluate(parser), 1);

  pn = parser.parse("1+tt[1+1]");
  assertEq(pn->evaluate(parser), 10);

  pn = parser.parse("a[3] = 12; tt.size() + a.size() + a[3]");
  assertEq(pn->evaluate(parser), 19);

  pn = parser.parse("a = 2; k = 1; while(a-- > 0) { k = k*2;} return k;");
  assertEq(pn->evaluate(parser), 4);

  pn = parser.parse("a=1; while(1) {a=a*2; if (a>60) break;} return a;");
  assertEq(pn->evaluate(parser), 64);

  pn = parser.parse("res = 1; for(k = 0; k < 10; ++k) res=res*2");
  assertEq(pn->evaluate(parser), 1024);

  pn = parser.parse("m = 1; n = 1; a = ++m + 4*n++; return a + 100*m + 1000*n");
  assertEq(pn->evaluate(parser), 2206);

  pn = parser.parse("m = 1; n = 1; a = m++ + 4*++n; return a + 100*m + 1000*n");
  assertEq(pn->evaluate(parser), 2209);

  pn = parser.parse("m = 2; n = 2; a = m-- + 4*--n; return a + 100*m + 1000*n");
  assertEq(pn->evaluate(parser), 1106);

  pn = parser.parse("for(m=0; m < 10; m++) arr[m] = m * m; ret = 0; for(m = 0; m < arr.size(); m++) ret = ret + arr[m];");
  assertEq(pn->evaluate(parser), 1+4+9+16+25+36+49+64+81);

  pn = parser.parse("a = 0; if (1!=2) a = a + 1; if (1<2) a=a+2; if (1<=2) a=a+4; if (1>2) a=a+8; if (!(1==2)) a=a+16; if (1>=2) a=a+32; if (1>5 or 1+1 < 5) a=a+64; return a;");
  assertEq(pn->evaluate(parser), 1+2+4+16+64);

  pn = parser.parse("i = 3; return ttt[1][i];"); 
  assertEq(pn->evaluate(parser), 55);

  pn = parser.parse("return ttt[3][0];"); 
  try {
    pn->evaluate(parser);
    assertEq(0,1);
  }
  catch (const meosException &) {
  }

  pn = parser.parse("return ttt[0][5];"); 
  try {
    pn->evaluate(parser);
    assertEq(0,1);
  }
  catch (const meosException &) {
  }

  pn = parser.parse("return ttt[4].size();"); 
  try {
    pn->evaluate(parser);
    assertEq(0,1);
  }
  catch (const meosException &) {
  }

  pn = parser.parse("ttt.size()"); 
  assertEq(pn->evaluate(parser), 2);

  pn = parser.parse("sum = 0; for (k = 0; k < ttt.size(); k++) {for (m = 0; m < ttt[k].size(); m++) sum = sum + ttt[k][m];}"); 
  assertEq(pn->evaluate(parser), 83);

  pn = parser.parse("ma = tt2; ma.sort(); s = 0; for(k = 0; k < ma.size(); k++) {s = s + (k+1)*ma[k];} return s;"); 
  pn->evaluate(parser);
  assertEq(pn->evaluate(parser), 1*1+2*2+3*3+4*4);

  pn = parser.parse("arr = ttt[1]; s = 0; for(k = 0; k < arr.size(); k++) {s = s + arr[k];} return s;"); 
  pn->evaluate(parser);
  assertEq(pn->evaluate(parser), 69);

  pn = parser.parse("k=5; //Test\n//Info\nreturn k+1;//Return 7;"); 
  pn->evaluate(parser);
  assertEq(pn->evaluate(parser), 6);

  pn = parser.parse("if (1>2) return 10; else if (1<2) return 11; else return 5;"); 
  pn->evaluate(parser);
  assertEq(pn->evaluate(parser), 11);

  try {
    parser.parse("if (1>2) return 10; else return 11; else return 5;"); 
    assertEq(0,1);
  }
  catch (const meosException &) {
  }

  try {
    parser.parse("return 10; else return 11;"); 
    assertEq(0,1);
  }
  catch (const meosException &) {
  }

}

void Parser::dumpVariables(gdioutput &gdi, int c1, int c2) const {
  for (map<string, vector<int> >::iterator it = var.begin(); it != var.end(); ++it) {
    const vector<int> &v = it->second;
    string val;
    if (v.size() == 1) {
      val = itos(v[0]);
    }
    else {
      val = "[";
      for (size_t k = 0; k < v.size(); k++) {
        if (k > 0)
          val += ",";
        val += itos(v[k]);
      }
      val += "]";
    }
    int cy = gdi.getCY();
    gdi.addStringUT(cy, c1, monoText, it->first, c2-c1-10);
    gdi.addStringUT(cy, c2, monoText, val);
  }
}

void Parser::dumpSymbols(gdioutput &gdi, int c1, int c2) const {
  for (map<string, Symbol>::const_iterator it = symb.begin(); it != symb.end(); ++it) {
    const vector< vector<int> > &v = it->second.value;
    if (v.empty())
      continue;

    int cy = gdi.getCY();
    gdi.addStringUT(cy, c1,  monoText, it->first, c2-c1-10);

    string val;

    if (v.size() == 1) {
      if (v[0].size() == 1) {
        val = itos(v[0][0]);
      }
      else {
        val = "[";
        for (size_t k = 0; k < v[0].size(); k++) {
          if (k > 0)
            val += ",";
          val += itos(v[0][k]);
        }
        val += "]";
      }
      gdi.addStringUT(cy, c2,  monoText, val);
    }
    else {
      for (size_t j = 0; j < v.size(); j++) {
        val = itos(j) + ": [";
        for (size_t k = 0; k < v[j].size(); k++) {
          if (k > 0)
            val += ",";
          val += itos(v[j][k]);
        }
        val += "]";
        gdi.addStringUT(cy, c2, monoText, val);
        cy = gdi.getCY();
      }
    }
  }
}
