#include <boost/spirit/include/k-map.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/variant/recursive_wrapper.hpp>

namespace qi    = boost::spirit::qi;
namespace phx   = boost::phoenix;



// Abstract data type

struct op_or  {};
struct op_and {};
struct op_imp {};
struct op_iff {};
struct op_not {};

typedef std::string var;
template <typename tag> struct binop;
template <typename tag> struct unop;

typedef boost::variant<var,
        boost::recursive_wrapper<unop <op_not> >,
        boost::recursive_wrapper<binop<op_and> >,
        boost::recursive_wrapper<binop<op_or> >,
        boost::recursive_wrapper<binop<op_imp> >,
        boost::recursive_wrapper<binop<op_iff> >
        > expr;

template <typename tag> struct binop
{
  explicit binop(const expr& l, const expr& r) : oper1(l), oper2(r) { }
  expr oper1, oper2;
};

template <typename tag> struct unop
{
  explicit unop(const expr& o) : oper1(o) { }
  expr oper1;
};



// Operating on the syntax tree

struct printer : boost::static_visitor<void>
{
  printer(std::ostream& os) : _os(os) {}
  std::ostream& _os;

  //
  void operator()(const var& v) const { _os << v; }

  void operator()(const binop<op_and>& b) const { print(" and ", b.oper1, b.oper2); }
  void operator()(const binop<op_or >& b) const { print(" or ", b.oper1, b.oper2); }
  void operator()(const binop<op_iff>& b) const { eliminate_iff(b.oper1, b.oper2); }
  void operator()(const binop<op_imp>& b) const { eliminate_imp(b.oper1, b.oper2); }

  void print(const std::string& op, const expr& l, const expr& r) const
  {
    _os << "(";
    boost::apply_visitor(*this, l);
    _os << op;
    boost::apply_visitor(*this, r);
    _os << ")";
  }

  void operator()(const unop<op_not>& u) const
  {
    _os << "( not ";
    boost::apply_visitor(*this, u.oper1);
    _os << ")";
  }

  void eliminate_iff(const expr& l, const expr& r) const
  {
    _os << "(";
    boost::apply_visitor(*this, l);
    _os << " or not ";
    boost::apply_visitor(*this, r);
    _os << ") and ( not ";
    boost::apply_visitor(*this, l);
    _os << " or ";
    boost::apply_visitor(*this, r);
    _os << ")";
  }

  void eliminate_imp(const expr& l, const expr& r) const
  {
    _os << "( not ";
    boost::apply_visitor(*this, l);
    _os << " or ";
    boost::apply_visitor(*this, r);
    _os << ")";
  }
};

std::ostream& operator<<(std::ostream& os, const expr& e)
{ boost::apply_visitor(printer(os), e); return os; }



// Grammar rules

template <typename It, typename Skipper = qi::space_type>
struct parser : qi::grammar<It, expr(), Skipper>
{
  parser() : parser::base_type(expr_)
  {
    using namespace qi;

    expr_  = iff_.alias();

    iff_ = (imp_ >> "iff" >> iff_) [ val = phx::construct<binop<op_iff>>(_1, _2) ] | imp   [ _val = _1 ];
    imp_ = (or_  >> "imp" >> imp_) [ val = phx::construct<binop<op_imp>>(_1, _2) ] | or    [ _val = _1 ];
    or_  = (and_ >> "or"  >> or_ ) [ val = phx::construct<binop<op_or >>(_1, _2) ] | and   [ _val = _1 ];
    and_ = (not_ >> "and" >> and_) [ val = phx::construct<binop<op_and>>(_1, _2) ] | not   [ _val = _1 ];
    not_ = ("not" > simple       ) [ _val = phx::construct<unop <op_not>>(_1)     ] | simple [ _val = _1 ];

    simple = (('(' > expr_ > ')') | var_);
    var_ = qi::lexeme[ +alpha ];

    BOOST_SPIRIT_DEBUG_NODE(expr_);
    BOOST_SPIRIT_DEBUG_NODE(iff_);
    BOOST_SPIRIT_DEBUG_NODE(imp_);
    BOOST_SPIRIT_DEBUG_NODE(or_);
    BOOST_SPIRIT_DEBUG_NODE(and_);
    BOOST_SPIRIT_DEBUG_NODE(not_);
    BOOST_SPIRIT_DEBUG_NODE(simple);
    BOOST_SPIRIT_DEBUG_NODE(var_);
  }

  private:
  qi::rule<It, var() , Skipper> var_;
  qi::rule<It, expr(), Skipper> not_, and_, or_, imp_, iff_, simple, expr_;
};




// Test some examples in main and check the order of precedence

int main()
{
  for (auto& input : std::list<std::string> {

      // Test the order of precedence
      "(a and b) imp ((c and d) or (a and b));",
      "a and b iff (c and d or a and b);",
      "a and b imp (c and d or a and b);",
      "not a or not b;",
      "a or b;",
      "not a and b;",
      "not (a and b);",
      "a or b or c;",
      "aaa imp bbb iff ccc;",
      "aaa iff bbb imp ccc;",


      // Test elimination of equivalences
      "a iff b;",
      "a iff b or c;",
      "a or b iff b;",
      "a iff b iff c;",

      // Test elimination of implications
      "p imp q;",
      "p imp not q;",
      "not p imp not q;",
      "p imp q and r;",
      "p imp q imp r;",
      })
  {
    auto f(std::begin(input)), l(std::end(input));
    parser<decltype(f)> p;

    try
    {
      expr result;
      bool ok = qi::phrase_parse(f,l,p > ';',qi::space,result);

      if (!ok)
        std::cerr << "invalid input\n";
      else
        std::cout << "result: " << result << "\n";

    } catch (const qi::expectation_failure<decltype(f)>& e)
    {
      std::cerr << "expectation_failure at '" << std::string(e.first, e.last) << "'\n";
    }

    if (f!=l) std::cerr << "unparsed: '" << std::string(f,l) << "'\n";
  }

  return 0;
}
