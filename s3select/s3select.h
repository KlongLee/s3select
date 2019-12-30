#include <boost/spirit/include/classic_core.hpp>
#include <iostream>
#include <string>
#include <list>
#include "s3select_oper.h"
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <functional>

using namespace std;
using namespace BOOST_SPIRIT_CLASSIC_NS;
#define _DEBUG_TERM {string  token(a,b);std::cout << __FUNCTION__ << token << std::endl;}

/// AST builder

struct actionQ
{

    list<mulldiv_operation::muldiv_t> muldivQ;
    list<addsub_operation::addsub_op_t> addsubQ;
    list<arithmetic_operand::cmp_t> arithmetic_compareQ;
    list<logical_operand::oplog_t> logical_compareQ;
    list<base_statement *> exprQ;
    list<base_statement *> funcQ;
    list<base_statement *> condQ;
    std::string from_clause;
    list<std::string> schema_columns;
    list<base_statement *> projections;

    void clear() {} //TBD clear all Q's between different queries;
};

class base_action
{

public:
    base_action() : m_action(0) {}
    actionQ *m_action;
    void set_action_q(actionQ *a) { m_action = a; }
};

struct push_from_clause : public base_action
{
    void operator()(const char *a, const char *b) const
    {
        std::string token(a, b);
        m_action->from_clause = token;
    }

} g_push_from_clause;

struct push_number : public base_action //TODO use define for defintion of actions
{
    int number;

    void operator()(const char *a, const char *b) const
    {
        string token(a, b);
        variable *v = new variable(atoi(token.c_str()));

        m_action->exprQ.push_back(v);
    }

} g_push_number;

struct push_variable : public base_action
{
    void operator()(const char *a, const char *b) const
    {
        string token(a, b);
        variable *v = new variable(token.c_str());

        m_action->exprQ.push_back(v);
    }
} g_push_variable;

/////////////////////////arithmetic unit  /////////////////
struct push_addsub : public base_action
{
    void operator()(const char *a, const char *b) const
    {
        string token(a, b);

        if (token.compare("+") == 0)
            m_action->addsubQ.push_back(addsub_operation::addsub_op_t::ADD);
        else
            m_action->addsubQ.push_back(addsub_operation::addsub_op_t::SUB);
    }
} g_push_addsub;

struct push_mulop : public base_action
{
    void operator()(const char *a, const char *b) const
    {
        string token(a, b);

        if (token.compare("*") == 0)
            m_action->muldivQ.push_back(mulldiv_operation::muldiv_t::MULL);
        else if (token.compare("/") == 0)
            m_action->muldivQ.push_back(mulldiv_operation::muldiv_t::DIV);
        else
            m_action->muldivQ.push_back(mulldiv_operation::muldiv_t::POW);
    }
} g_push_mulop;

struct push_addsub_binop : public base_action
{
    void operator()(const char *a, const char *b) const
    {
        base_statement *l = 0, *r = 0;

        r = m_action->exprQ.back();
        m_action->exprQ.pop_back();
        l = m_action->exprQ.back();
        m_action->exprQ.pop_back();
        addsub_operation::addsub_op_t o = m_action->addsubQ.back();
        m_action->addsubQ.pop_back();
        addsub_operation *as = new addsub_operation(l, o, r);
        m_action->exprQ.push_back(as);
    }
} g_push_addsub_binop;

struct push_mulldiv_binop : public base_action
{
    void operator()(const char *a, const char *b) const
    {
        base_statement *vl = 0, *vr = 0;

        vr = m_action->exprQ.back();
        m_action->exprQ.pop_back();
        vl = m_action->exprQ.back();
        m_action->exprQ.pop_back();
        mulldiv_operation::muldiv_t o = m_action->muldivQ.back();
        m_action->muldivQ.pop_back();
        mulldiv_operation *f = new mulldiv_operation(vl, o, vr);
        m_action->exprQ.push_back(f);
    }
} g_push_mulldiv_binop;

struct push_function_arg : public base_action
{
    void operator()(const char *a, const char *b) const
    {
        base_statement *be = m_action->exprQ.back();
        m_action->exprQ.pop_back();
        base_statement *f = m_action->funcQ.back();

        if (dynamic_cast<__function *>(f))
            dynamic_cast<__function *>(f)->push_argument(be);
    }
} g_push_function_arg;

struct push_function_name : public base_action
{
    void operator()(const char *a, const char *b) const
    {
        std::string token(a, b);

        __function *func = new __function(token.c_str());

        m_action->funcQ.push_back(func);
    }
} g_push_function_name;

struct push_function_expr : public base_action
{
    void operator()(const char *a, const char *b) const
    {
        base_statement *func = m_action->funcQ.back();
        m_action->funcQ.pop_back();

        m_action->exprQ.push_back(func);
    }
} g_push_function_expr;

////////////////////// logical unit ////////////////////////

struct push_compare_operator : public base_action
{
    void operator()(const char *a,const char *b) const
    {
        string token(a, b);
        arithmetic_operand::cmp_t c = arithmetic_operand::cmp_t::NA;

        if (token.compare("==") == 0)
            c = arithmetic_operand::cmp_t::EQ;
        else if (token.compare("!=") == 0)
            c = arithmetic_operand::cmp_t::NE;
        else if (token.compare(">=") == 0)
            c = arithmetic_operand::cmp_t::GE;
        else if (token.compare("<=") == 0)
            c = arithmetic_operand::cmp_t::LE;
        else if (token.compare(">") == 0)
            c = arithmetic_operand::cmp_t::GT;
        else if (token.compare("<") == 0)
            c = arithmetic_operand::cmp_t::LT;
        else
            c = arithmetic_operand::cmp_t::NA;

        m_action->arithmetic_compareQ.push_back(c);
    }
} g_push_compare_operator;

struct push_logical_operator : public base_action
{
    void operator()(const char *a,const char *b) const
    {
        string token(a, b);
        logical_operand::oplog_t l = logical_operand::oplog_t::NA;

        if (token.compare("and") == 0)
            l = logical_operand::oplog_t::AND;
        else if (token.compare("or") == 0)
            l = logical_operand::oplog_t::OR;
        else
            l = logical_operand::oplog_t::NA;

        m_action->logical_compareQ.push_back(l);

    }
} g_push_logical_operator;

struct push_arithmetic_predicate : public base_action
{
    void operator()(const char *a,const char *b) const
    {
        string token(a, b);

        base_statement *vr, *vl;
        arithmetic_operand::cmp_t c = m_action->arithmetic_compareQ.back();
        m_action->arithmetic_compareQ.pop_back();
        vr = m_action->exprQ.back();
        m_action->exprQ.pop_back();
        vl = m_action->exprQ.back();
        m_action->exprQ.pop_back();

        arithmetic_operand *t = new arithmetic_operand(vl, c, vr);

        m_action->condQ.push_back(t);
    }
} g_push_arithmetic_predicate;

struct push_logical_predicate : public base_action
{

    void operator()(const char *a,const char *b) const
    {
        string token(a, b);

        base_statement *tl = 0, *tr = 0;
        logical_operand::oplog_t oplog = m_action->logical_compareQ.back();
        m_action->logical_compareQ.pop_back();

        if (m_action->condQ.empty() == false)
        {
            tr = m_action->condQ.back();
            m_action->condQ.pop_back();
        }
        if (m_action->condQ.empty() == false)
        {
            tl = m_action->condQ.back();
            m_action->condQ.pop_back();
        }

        logical_operand *f = new logical_operand(tl, oplog, tr);

        m_action->condQ.push_back(f);
    }
} g_push_logical_predicate;

struct push_column_pos : public base_action
{
    void operator()(const char *a,const char *b) const
    {
        string token(a, b);
        variable *v = new variable(token.c_str() , variable::var_t::POS );

        m_action->exprQ.push_back(v);
    }

} g_push_column_pos;

struct push_projection : public base_action
{
    void operator()(const char *a,const char *b) const
    {
        string token(a, b);

        m_action->projections.push_back( m_action->exprQ.back() );
        m_action->exprQ.pop_back();
    }

} g_push_projection;

/// for the schema description "mini-parser"
struct push_column : public base_action
{

    void operator()(const char *a,const char *b) const
    {
        std::string token(a,b);

        m_action->schema_columns.push_back(token); 
    }

} g_push_column;

struct s3select : public grammar<s3select>
{
    private:

    actionQ m_actionQ; //TODO on heap

    scratch_area m_sca;//TODO on heap

    #define BOOST_BIND_ACTION( push_name ) boost::bind( &push_name::operator(), g_ ## push_name , _1 ,_2)
    #define ATTACH_ACTION_Q( push_name ) {(g_ ## push_name).set_action_q(&m_actionQ);}

    public:

    s3select()
    {   //TODO check option for defining action and push into list 
        ATTACH_ACTION_Q(push_from_clause);
        ATTACH_ACTION_Q(push_number);
        ATTACH_ACTION_Q(push_logical_operator);
        ATTACH_ACTION_Q(push_logical_predicate);
        ATTACH_ACTION_Q(push_compare_operator);
        ATTACH_ACTION_Q(push_arithmetic_predicate);
        ATTACH_ACTION_Q(push_addsub);
        ATTACH_ACTION_Q(push_addsub_binop);
        ATTACH_ACTION_Q(push_mulop);
        ATTACH_ACTION_Q(push_mulldiv_binop);
        ATTACH_ACTION_Q(push_function_arg);
        ATTACH_ACTION_Q(push_function_name);
        ATTACH_ACTION_Q(push_function_expr);
        ATTACH_ACTION_Q(push_number);
        ATTACH_ACTION_Q(push_variable);
        ATTACH_ACTION_Q(push_column_pos);
        ATTACH_ACTION_Q(push_projection);

    }

    bool is_semantic()//TBD traverse and validate semantics per all nodes
    {
        base_statement * cond = m_actionQ.exprQ.back();

        bool res = cond->semantic();
    }
    
    std::string get_from_clause()
    {
        return m_actionQ.from_clause;
    }

    void load_schema(std::list<string> &scm)
    {
        int i = 0;
        for (auto c : scm)
            m_sca.set_column_pos(c.c_str(), i++);
    }

    base_statement* get_filter()
    {
        return m_actionQ.condQ.back();
    }

    std::list<base_statement*>  get_projections_list()
    {
        return m_actionQ.projections; //TODO return COPY(?) or to return evalaution results (list of class value{})
    }

    scratch_area* get_scratch_area()
    {
        return &m_sca;
    }

    template <typename ScannerT>
    struct definition
    {
        definition(s3select const & )
        {///// s3select syntax rules and actions for building AST

            select_expr = str_p("select")  >> projections >> str_p("from") >> (s3_object)[BOOST_BIND_ACTION(push_from_clause)] >> !where_clause >> ';';
            
            projections = projection_expression >> *( ',' >> projection_expression) ;

            projection_expression = (arithmetic_expression >> str_p("as") >> alias_name) | (arithmetic_expression)[BOOST_BIND_ACTION(push_projection)]  ;

            alias_name = lexeme_d[(+alpha_p >> *digit_p)] ;


            s3_object = str_p("stdin") | object_path ; 

            object_path = "/" >> *( fs_type >> "/") >> fs_type;

            fs_type = lexeme_d[+( alnum_p | str_p(".")  | str_p("_")) ];

            where_clause = str_p("where") >> condition_expression;

            condition_expression = (arithmetic_predicate >> *(log_op[BOOST_BIND_ACTION(push_logical_operator)] >> arithmetic_predicate[BOOST_BIND_ACTION(push_logical_predicate)]));

            arithmetic_predicate = (factor >> *(arith_cmp[BOOST_BIND_ACTION(push_compare_operator)] >> factor[BOOST_BIND_ACTION(push_arithmetic_predicate)]));

            factor = (arithmetic_expression) | ('(' >> condition_expression >> ')') ; 

            arithmetic_expression = (addsub_operand >> *(addsubop_operator[BOOST_BIND_ACTION(push_addsub)] >> addsub_operand[BOOST_BIND_ACTION(push_addsub_binop)] ));

            addsub_operand = (mulldiv_operand >> *(muldiv_operator[BOOST_BIND_ACTION(push_mulop)]  >> mulldiv_operand[BOOST_BIND_ACTION(push_mulldiv_binop)] ));// this non-terminal gives precedense to  mull/div

            mulldiv_operand = arithmetic_argument | ('(' >> (arithmetic_expression) >> ')') ; 

            list_of_function_arguments = (arithmetic_expression)[BOOST_BIND_ACTION(push_function_arg)] >> *(',' >> (arithmetic_expression)[BOOST_BIND_ACTION(push_function_arg)]) ;

            function = variable[BOOST_BIND_ACTION(push_function_name)] >> ( '(' >> list_of_function_arguments >> ')' ) [BOOST_BIND_ACTION(push_function_expr)];
            
            arithmetic_argument = (number)[BOOST_BIND_ACTION(push_number)] | (column_pos)[BOOST_BIND_ACTION(push_column_pos)] | (function)| (variable)[BOOST_BIND_ACTION(push_variable)];//function is pushed by right-term 

                       
            number = int_p; //TODO float , string 

            column_pos = ('_'>>+(digit_p) ) ;

            muldiv_operator = str_p("*") | str_p("/") | str_p("^");// got precedense

            addsubop_operator = str_p("+") | str_p("-");


            arith_cmp = str_p(">=") | str_p("<=") | str_p("==") | str_p("<") | str_p(">") | str_p("!=");

            log_op = str_p("and") | str_p("or"); //TODO add NOT (unary)

            variable =  lexeme_d[(+alpha_p >> *digit_p)];
        }
    

        rule<ScannerT> variable, select_expr, s3_object, where_clause, number, arith_cmp, log_op, condition_expression, arithmetic_predicate, factor;
        rule<ScannerT> muldiv_operator, addsubop_operator, function, arithmetic_expression, addsub_operand, list_of_function_arguments, arithmetic_argument, mulldiv_operand;
        rule<ScannerT> fs_type,object_path;
        rule<ScannerT> projections,projection_expression,alias_name,column_pos;
        rule<ScannerT> const &start() const { return select_expr; }
    };
};

/////// handling different object types
class base_s3object
{

protected:
    scratch_area *m_sa;
    std::string m_obj_name;

public:
    base_s3object(const char *obj_name, scratch_area *m) : m_sa(m), m_obj_name(obj_name) {}

    virtual int getNextRow(char **) = 0; //fetch next row
};