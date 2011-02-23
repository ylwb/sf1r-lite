///
/// @brief  source file of Query Parser
/// @author Dohyun Yun
/// @date   2010.06.16 (First Created)
///

#include "QueryManager.h"
#include "QueryParser.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>

namespace sf1r
{

    using izenelib::util::UString;

    // Declare static variables
    std::string QueryParser::operStr_;
    std::string QueryParser::escOperStr_;
    boost::unordered_map<std::string , std::string> QueryParser::operEncodeDic_;
    boost::unordered_map<std::string , std::string> QueryParser::operDecodeDic_;
    boost::unordered_map<char, bool> QueryParser::openBracket_;
    boost::unordered_map<char, bool> QueryParser::closeBracket_;

    QueryParser::QueryParser(
            boost::shared_ptr<LAManager>& laManager,
            boost::shared_ptr<izenelib::ir::idmanager::IDManager>& idManager ):
        grammar<QueryParser>(), laManager_(laManager), idManager_(idManager)
    {
    } // end - QueryParser()

    void QueryParser::initOnlyOnce()
    {
        static boost::once_flag once = BOOST_ONCE_INIT;
        boost::call_once(&initOnlyOnceCore, once);
    } // end - initOnlyOnce()

    void QueryParser::initOnlyOnceCore()
    {
        operStr_.assign(" |!(){}[]^\"");
        escOperStr_ = operStr_; escOperStr_ += '\\';

        operEncodeDic_.insert( make_pair( "\\\\"    , "::$OP_SL$::" ) ); // "\\" : Operator Slash
        operEncodeDic_.insert( make_pair( "\\ "     , "::$OP_AN$::" ) ); // "\ " : Operator AND
        operEncodeDic_.insert( make_pair( "\\|"     , "::$OP_OR$::" ) ); // "\|" : Operator OR
        operEncodeDic_.insert( make_pair( "\\!"     , "::$OP_NT$::" ) ); // "\!" : Operator NOT
        operEncodeDic_.insert( make_pair( "\\("     , "::$OP_BO$::" ) ); // "\(" : Operator Opening Bracket
        operEncodeDic_.insert( make_pair( "\\)"     , "::$OP_BC$::" ) ); // "\)" : Operator Closing Bracket
        operEncodeDic_.insert( make_pair( "\\["     , "::$OP_OO$::" ) ); // "\[" : Operator Opening Orderby Bracket
        operEncodeDic_.insert( make_pair( "\\]"     , "::$OP_OC$::" ) ); // "\]" : Operator Closing Orderby Bracket
        operEncodeDic_.insert( make_pair( "\\{"     , "::$OP_NO$::" ) ); // "\{" : Operator Opening Nearby Bracket
        operEncodeDic_.insert( make_pair( "\\}"     , "::$OP_NC$::" ) ); // "\}" : Operator Closing Nearby Bracket
        operEncodeDic_.insert( make_pair( "\\^"     , "::$OP_UP$::" ) ); // "\^" : Operator Upper Arrow
        operEncodeDic_.insert( make_pair( "\\\""    , "::$OP_EX$::" ) ); // "\"" : Operator Exact Bracket

        operDecodeDic_.insert( make_pair( "::$OP_SL$::" , "\\" ) ); // "\" : Operator Slash
        operDecodeDic_.insert( make_pair( "::$OP_AN$::" , " "  ) ); // " " : Operator AND
        operDecodeDic_.insert( make_pair( "::$OP_OR$::" , "|"  ) ); // "|" : Operator OR
        operDecodeDic_.insert( make_pair( "::$OP_NT$::" , "!"  ) ); // "!" : Operator NOT
        operDecodeDic_.insert( make_pair( "::$OP_BO$::" , "("  ) ); // "(" : Operator Opening Bracket
        operDecodeDic_.insert( make_pair( "::$OP_BC$::" , ")"  ) ); // ")" : Operator Closing Bracket
        operDecodeDic_.insert( make_pair( "::$OP_OO$::" , "["  ) ); // "[" : Operator Opening Orderby Bracket
        operDecodeDic_.insert( make_pair( "::$OP_OC$::" , "]"  ) ); // "]" : Operator Closing Orderby Bracket
        operDecodeDic_.insert( make_pair( "::$OP_NO$::" , "{"  ) ); // "{" : Operator Opening Nearby Bracket
        operDecodeDic_.insert( make_pair( "::$OP_NC$::" , "}"  ) ); // "}" : Operator Closing Nearby Bracket
        operDecodeDic_.insert( make_pair( "::$OP_UP$::" , "^"  ) ); // "^" : Operator Upper Arrow
        operDecodeDic_.insert( make_pair( "::$OP_EX$::" , "\"" ) ); // """ : Operator Exact Bracket

        using namespace std;
        openBracket_.insert( make_pair('(',true) );
        openBracket_.insert( make_pair('[',true) );
        openBracket_.insert( make_pair('{',true) );
        openBracket_.insert( make_pair('"',true) );

        closeBracket_.insert( make_pair(')',true) );
        closeBracket_.insert( make_pair(']',true) );
        closeBracket_.insert( make_pair('}',true) );
        closeBracket_.insert( make_pair('"',true) );

    } // end - initOnlyOnceCore()


    void QueryParser::processEscapeOperator(std::string& queryString)
    {
        processReplaceAll(queryString, operEncodeDic_);
    } // end - processEscapeOperator()

    void QueryParser::recoverEscapeOperator(std::string& queryString)
    {
        processReplaceAll(queryString, operDecodeDic_);
    } // end - recoverEscapeOperator()

    void QueryParser::addEscapeCharToOperator(std::string& queryString)
    {
        std::string tmpQueryStr;
        for(std::string::iterator iter = queryString.begin(); iter != queryString.end(); iter++)
        {
            if ( escOperStr_.find(*iter) != std::string::npos )
                tmpQueryStr += "\\";
            tmpQueryStr += *iter;
        } // end - for
        queryString.swap( tmpQueryStr );
    } // end - addEscapeCharToOperator()

    void QueryParser::removeEscapeChar(std::string& queryString)
    {
        std::string tmpQueryStr;
        for(std::string::iterator iter = queryString.begin(); iter != queryString.end(); iter++)
        {
            if ( *iter == '\\' )
            {
                iter++;
                if ( iter == queryString.end() )
                    break;
                else if ( escOperStr_.find(*iter) != std::string::npos )
                    tmpQueryStr += *iter;
                else
                {
                    tmpQueryStr += '\\';
                    tmpQueryStr += *iter;
                }
                continue;
            }
            tmpQueryStr += *iter;
        } // end - for
        queryString.swap( tmpQueryStr );

    } // end - addEscapeCharToOperator()

    void QueryParser::normalizeQuery(const std::string& queryString, std::string& normString)
    {
        std::string tmpString, tmpNormString;
        std::string::const_iterator iter, iterEnd, prevIter;

        // -----[ Step 1 : Remove first & end spaces, change continuous spaces into one and the space before and after | ]
        iter = queryString.begin();
        iterEnd = queryString.end();

        // Remove first spaces
        if ( *iter == ' ' )
            while( ++iter != iterEnd && *iter == ' ');

        while ( iter != iterEnd )
        {
            if ( *iter == ' ' )
            {
                // (   hello kity   ) -> ( hello kity )
                // (  hello     | kity ) -> ( hello| kitty )
                while ( ++iter != iterEnd && *iter == ' ' );
                if ( iter != iterEnd && *iter != '|')
                    tmpString.push_back(' ');
            }
            else if ( *iter == '|' )
            {
                // (Hello|  kity) -> (Hello|kity)
                while ( ++iter != iterEnd && *iter == ' ' );
                tmpString.push_back('|');
            } // end - else if
            else tmpString.push_back( *iter++ );
        } // end - while

        // -----[ Step 2 : Remove or add space after or behind of specific operator ]
        iter = tmpString.begin();
        iterEnd = tmpString.end();
        while (iter != iterEnd)
        {
            switch (*iter)
            {
            case '!':
            case '|':
            case '(':
            case '[':
            case '{':
            case '}':
                // ( hello world) -> (hello world)
                tmpNormString.push_back( *iter++ );
                if ( iter != iterEnd && *iter == ' ' ) iter++;
                break;
            case ')':
            case ']':
                // (test keyword)attach -> (test keyword) attach
                tmpNormString.push_back( *iter++ );
                if ( iter != iterEnd && *iter != ' ' && *iter != '|')
                    tmpNormString.push_back(' ');
                break;
            case '^':
                // Remove space between ^ and number and add space between number and open bracket.
                // {Test case}^ 123(case) -> {Test case}^123 (case)
                tmpNormString.push_back( *iter++ );
                if ( iter != iterEnd && *iter == ' ' ) iter++;

                while ( iter != iterEnd && isdigit(*iter) ) // Store digit
                    tmpNormString.push_back( *iter++ );

                if ( openBracket_[*iter] ) // if first char after digit is open bracket, insert space.
                    tmpNormString.push_back(' ');
                break;
            case ' ': // (hello world ) -> (hello world)
                if ( ++iter != iterEnd && !closeBracket_[*iter] )
                    tmpNormString.push_back(' ');
                break;
            case '"': // Skip all things inside the exact bracket.
                tmpNormString.push_back('"');
                while ( ++iter != iterEnd && *iter != '"') tmpNormString.push_back( *iter );
                tmpNormString.push_back( *iter++ ); // insert last "
                break;
            default: // Store char && add space if openBracket is attached to the back of closeBracket.
                //prevIter = iter;
                tmpNormString.push_back( *iter++ );
                if ( (iter != iterEnd) && (openBracket_[*iter] || *iter == '!') )
                    tmpNormString.push_back(' ');
            } // end - switch()
        } // end - while

        normString.swap( tmpNormString );

    } // end - normalizeQuery()

    bool QueryParser::parseQuery( 
            const izenelib::util::UString& queryUStr, 
            QueryTreePtr& queryTree, 
            bool unigramFlag, 
            bool removeChineseSpace
            )
    {
        // Check if given query is restrict word.
        if ( QueryUtility::isRestrictWord( queryUStr ) )
            return false;

        std::string queryString, normQueryString;
        queryUStr.convertString(queryString, izenelib::util::UString::UTF_8);

        processEscapeOperator(queryString);
        normalizeQuery(queryString, normQueryString);

        // Remove redundant space for chinese character.
        if ( removeChineseSpace )
        {
            izenelib::util::UString spaceRefinedQuery;
            spaceRefinedQuery.assign(normQueryString, izenelib::util::UString::UTF_8);
            la::removeRedundantSpaces( spaceRefinedQuery );
            spaceRefinedQuery.convertString(normQueryString, izenelib::util::UString::UTF_8);
        }

        tree_parse_info<> info = ast_parse(normQueryString.c_str(), *this);

        if ( info.full )
        {
            if ( !getQueryTree(info.trees.begin(), queryTree, unigramFlag) )
                return false;
            queryTree->postProcess();
        }

        return info.full;
    } // end - parseQuery()

    bool QueryParser::getAnalyzedQueryTree(
        bool synonymExtension,
        const AnalysisInfo& analysisInfo,
        const izenelib::util::UString& rawUStr,
        QueryTreePtr& analyzedQueryTree,
        bool unigramFlag
    )
    {
        QueryTreePtr tmpQueryTree;
        LAEXInfo laInfo(unigramFlag, synonymExtension, analysisInfo);

        // Apply escaped operator.
        QueryParser::parseQuery( rawUStr, tmpQueryTree, unigramFlag );
        bool ret = recursiveQueryTreeExtension( tmpQueryTree, laInfo );
        if ( ret )
        {
            tmpQueryTree->postProcess();
            analyzedQueryTree = tmpQueryTree;
        }
        return ret;
    } // end - getPropertyQueryTree()

    bool QueryParser::extendUnigramWildcardTree(QueryTreePtr& queryTree)
    {
        std::string wildStr( queryTree->keyword_ );
        transform(wildStr.begin(), wildStr.end(), wildStr.begin(), ::tolower);
        UString wildcardUStringQuery(wildStr, UString::UTF_8);

        for (UString::iterator iter = wildcardUStringQuery.begin(); iter!= wildcardUStringQuery.end(); iter++)
        {
            if (*iter == '*')
            {
                queryTree->insertChild(QueryTreePtr(new QueryTree(QueryTree::ASTERISK)));
            }
            else if ( *iter == '?' )
            {
                queryTree->insertChild(QueryTreePtr(new QueryTree(QueryTree::QUESTION_MARK)));
            }
            else
            {
                QueryTreePtr child(new QueryTree(QueryTree::KEYWORD));

                child->keywordUString_ = *iter;
                // Skip the restrict term
                if ( !setKeyword(child, child->keywordUString_) )
                    continue;

                queryTree->insertChild(child);
            }
        }
        return true;
    } // end - extendWildcardTree

    bool QueryParser::extendTrieWildcardTree(QueryTreePtr& queryTree)
    {
        std::string wildStr( queryTree->keyword_ );
        transform(wildStr.begin(), wildStr.end(), wildStr.begin(), ::tolower);
        UString wildcardUStringQuery(wildStr, UString::UTF_8);

        std::vector<UString> wStrList;
        std::vector<termid_t> wIdList;
        idManager_->getTermListByWildcardPattern(wildcardUStringQuery, wStrList);
        idManager_->getTermIdListByTermStringList(wStrList, wIdList);

        // make child of wildcard
        std::vector<UString>::iterator strIter = wStrList.begin();
        std::vector<termid_t>::iterator idIter = wIdList.begin();
        for (; strIter != wStrList.end() && idIter != wIdList.end(); strIter++, idIter++)
        {
            // Skip the restrict term
            if ( QueryUtility::isRestrictId( *idIter ) )
                continue;
            QueryTreePtr child(new QueryTree(QueryTree::KEYWORD));
            child->keywordUString_ = *strIter;
            child->keywordId_ = *idIter;
            strIter->convertString( child->keyword_, UString::UTF_8 );

            queryTree->insertChild( child );
        }
        return true;
    } // end - extendWildcardTree

    bool QueryParser::recursiveQueryTreeExtension(QueryTreePtr& queryTree, const LAEXInfo& laInfo)
    {
        switch (queryTree->type_)
        {
        case QueryTree::KEYWORD:
        {
            QueryTreePtr tmpQueryTree;
            UString analyzedUStr;
            laManager_->getExpandedQuery(
                queryTree->keywordUString_,
                laInfo.analysisInfo_,
                true,
                laInfo.synonymExtension_,
                analyzedUStr);
            std::string escAddedStr;
            analyzedUStr.convertString(escAddedStr, UString::UTF_8);
            analyzedUStr.assign(escAddedStr, UString::UTF_8);
            if (!QueryParser::parseQuery(analyzedUStr, tmpQueryTree, laInfo.unigramFlag_, false))
                return false;
            queryTree = tmpQueryTree;
            break;
        }
        // Skip Language Analyzing
        case QueryTree::UNIGRAM_WILDCARD:
        case QueryTree::TRIE_WILDCARD:
        // Unlike NEARBY and ORDERED query, there is no need to analyze EXACT query.
        // EXACT query is processed via unigram.
        case QueryTree::EXACT:
            break;
        case QueryTree::NEARBY:
        case QueryTree::ORDER:
            return tokenizeBracketQuery(
                    queryTree->keyword_,
                    laInfo.analysisInfo_,
                    queryTree->type_,
                    queryTree,
                    queryTree->distance_);
        default:
            for (QTIter iter = queryTree->children_.begin();
                    iter != queryTree->children_.end(); iter++)
                recursiveQueryTreeExtension(*iter, laInfo);
        } // end - switch(queryTree->type_)
        return true;
    } // end - recursiveQueryTreeExtension()

    bool QueryParser::getQueryTree(iter_t const& i, QueryTreePtr& queryTree, bool unigramFlag)
    {
        if ( i->value.id() == stringQueryID )
            return processKeywordAssignQuery(i, queryTree, unigramFlag);
        else if  (i->value.id() == exactQueryID )
            return processExactQuery(i, queryTree);
        else if ( i->value.id() == orderedQueryID )
            return processBracketQuery(i, QueryTree::ORDER, queryTree, unigramFlag);
        else if ( i->value.id() == nearbyQueryID )
            return processBracketQuery(i, QueryTree::NEARBY, queryTree, unigramFlag);
        else if ( i->value.id() == boolQueryID )
            return processBoolQuery(i, queryTree, unigramFlag);
        else
            return processChildTree(i, queryTree, unigramFlag);
    } // end - getQueryTree()

    bool QueryParser::processKeywordAssignQuery( iter_t const& i, QueryTreePtr& queryTree, bool unigramFlag)
    {
        bool ret = true;

        std::string tmpString( i->value.begin(), i->value.end() );
        recoverEscapeOperator( tmpString );
        QueryTreePtr tmpQueryTree(new QueryTree(QueryTree::KEYWORD));
        if ( !setKeyword(tmpQueryTree, tmpString) )
            return false;

        if ( tmpString.find('*') != std::string::npos || tmpString.find('?') != std::string::npos )
        {
            if (unigramFlag)
            {
                tmpQueryTree->type_ = QueryTree::UNIGRAM_WILDCARD;
                ret = extendUnigramWildcardTree(tmpQueryTree);
            }
            else
            {
                tmpQueryTree->type_ = QueryTree::TRIE_WILDCARD;
                ret = extendTrieWildcardTree(tmpQueryTree);
            }
        }

        queryTree = tmpQueryTree;
        return ret;
    } // end - processKeywordAssignQuery()

    bool QueryParser::processExactQuery(iter_t const& i, QueryTreePtr& queryTree)
    {
        bool ret;
        QueryTreePtr tmpQueryTree(new QueryTree(QueryTree::EXACT));

        // Store String value of first child into keyword_
        iter_t childIter = i->children.begin();
        std::string tmpString( childIter->value.begin(), childIter->value.end() );
        setKeyword(tmpQueryTree, tmpString);

        // Make child query tree with default-tokenized terms.
        la::TermList termList;
        AnalysisInfo analysisInfo;

        // Unigram analyzing is used for any kinds of query for "EXACT" query.
        analysisInfo.analyzerId_ = "la_unigram"; 
        ret = laManager_->getTermList(
            tmpQueryTree->keywordUString_,
            analysisInfo,
//            false,
            termList );
        if ( !ret ) return false;

        cout << "ExactQuery Term : " << la::to_utf8(tmpQueryTree->keywordUString_) << endl;
        cout << "ExactQuery TermList size : " << termList.size() << endl;
        cout << "------------------------------" << endl << termList << endl;

        la::TermList::iterator termIter = termList.begin();

        while(termIter != termList.end() ) {
            QueryTreePtr child(new QueryTree(QueryTree::KEYWORD));
            child->keywordUString_ = termIter->text_;
            setKeyword(child, child->keywordUString_);
            tmpQueryTree->insertChild(child);
            termIter ++;
        }

        queryTree = tmpQueryTree;
        return true;
    }

    bool QueryParser::processBracketQuery(iter_t const& i, QueryTree::QueryType queryType, QueryTreePtr& queryTree, bool unigramFlag)
    {
        // Store String value of first child into keyword_
        iter_t childIter = i->children.begin();
        std::string queryStr( childIter->value.begin(), childIter->value.end() );

        // Use default tokenizer
        AnalysisInfo analysisInfo;
        int distance;
        if ( queryType == QueryTree::NEARBY )
        {
            // Store distance
            iter_t distIter = i->children.begin()+1;
            if ( distIter != i->children.end() )
            {
                std::string distStr( distIter->value.begin(), distIter->value.end() );
                distance = atoi( distStr.c_str() );
            }
            else
                distance = 20;
        }
        return tokenizeBracketQuery(queryStr, analysisInfo, queryType, queryTree, distance);
    } // end - processBracketQuery()

    bool QueryParser::tokenizeBracketQuery(
            const std::string& queryStr,
            const AnalysisInfo& analysisInfo,
            QueryTree::QueryType queryType,
            QueryTreePtr& queryTree,
            int distance)
    {
        bool ret;
        QueryTreePtr tmpQueryTree(new QueryTree(queryType));
        setKeyword(tmpQueryTree, queryStr);

        // Make child query tree with default-tokenized terms.
        la::TermList termList;
        la::TermList::iterator termIter;
        ret = laManager_->getTermList(
            tmpQueryTree->keywordUString_,
            analysisInfo,
//            false,
            termList );
        if ( !ret ) return false;

        for ( termIter = termList.begin(); termIter != termList.end(); termIter++ )
        {
            // Find restrict Word
            if ( QueryUtility::isRestrictWord( termIter->text_ ) )
                continue;

            QueryTreePtr child(new QueryTree(QueryTree::KEYWORD));
            termIter->text_.convertString(child->keyword_, UString::UTF_8);
            removeEscapeChar(child->keyword_);
            setKeyword(child, child->keyword_);
            tmpQueryTree->insertChild( child );
        }

        if ( queryType == QueryTree::NEARBY )
            tmpQueryTree->distance_ = distance;

        queryTree = tmpQueryTree;
        return true;
    } // end - tokenizeBracketQuery()

    bool QueryParser::processBoolQuery(iter_t const& i, QueryTreePtr& queryTree, bool unigramFlag)
    {
        QueryTree::QueryType queryType;
        switch ( *(i->value.begin() ) )
        {
        case ' ':
            queryType = QueryTree::AND;
            break;
        case '|':
            queryType = QueryTree::OR;
            break;
        default:
            queryType = QueryTree::UNKNOWN;
            return false;
        } // end - switch
        QueryTreePtr tmpQueryTree( new QueryTree(queryType) );

        // Retrieve child query tree and insert it to the children_ list of tmpQueryTree.
        iter_t iter = i->children.begin();
        for (; iter != i->children.end(); iter++)
        {
            QueryTreePtr tmpChildQueryTree;
            if ( getQueryTree( iter, tmpChildQueryTree, unigramFlag) )
                tmpQueryTree->insertChild( tmpChildQueryTree );
            else
                return false;
        }

        // Merge unnecessarily divided bool-trees
        //   - Children are always two : Binary Tree
        QTIter first = tmpQueryTree->children_.begin();
        QTIter second = first;
        second++;
        if ( (*first)->type_ == queryType )
        {
            // use first child tree as a root tree.
            if ( (*second)->type_ == queryType )
                // Move all children in second node into first ( Insert : first->end )
                (*first)->children_.splice( (*first)->children_.end(), (*second)->children_ );
            else
                // Move second node into children in first ( Insert : first->end )
                (*first)->children_.push_back( *second );

            tmpQueryTree = *first;
        }
        else if ((*second)->type_ == queryType )
        {
            // Move first node into children in second. ( Insert : second->begin )
            (*second)->children_.push_front( *first );
            tmpQueryTree = *second;
        }

        queryTree = tmpQueryTree;
        return true;
    } // end - processBoolQuery()

    bool QueryParser::processChildTree(iter_t const& i, QueryTreePtr& queryTree, bool unigramFlag)
    {
        QueryTree::QueryType queryType;
        if ( i->value.id() == notQueryID ) queryType = QueryTree::NOT;
        else queryType = QueryTree::UNKNOWN;

        QueryTreePtr tmpQueryTree( new QueryTree(queryType) );

        // Retrieve child query tree and insert it to the children_ list of tmpQueryTree.
        iter_t iter = i->children.begin();
        for (; iter != i->children.end(); iter++)
        {
            QueryTreePtr tmpChildQueryTree;
            if ( getQueryTree( iter, tmpChildQueryTree, unigramFlag) )
                tmpQueryTree->insertChild( tmpChildQueryTree );
            else
                return false;
        }
        queryTree = tmpQueryTree;
        return true;
    } // end - processChildTree

    bool QueryParser::processReplaceAll(std::string& queryString, boost::unordered_map<std::string,std::string>& dic)
    {
        boost::unordered_map<std::string,std::string>::const_iterator iter = dic.begin();
        for(; iter != dic.end(); iter++)
            boost::replace_all(queryString, iter->first, iter->second);
        return true;
    } // end - process Rreplace All process

    bool QueryParser::setKeyword(QueryTreePtr& queryTree, const std::string& utf8Str)
    {
        if ( !queryTree )
            return false;
        queryTree->keyword_ = utf8Str;
        queryTree->keywordUString_.assign( utf8Str , UString::UTF_8 );
        idManager_->getTermIdByTermString( queryTree->keywordUString_ , queryTree->keywordId_ );
        return !QueryUtility::isRestrictWord( queryTree->keywordUString_ );
    } // end - setKeyword()

    bool QueryParser::setKeyword(QueryTreePtr& queryTree, const izenelib::util::UString& uStr)
    {
        if ( !queryTree )
            return false;
        queryTree->keywordUString_.assign( uStr );
        queryTree->keywordUString_.convertString(queryTree->keyword_ , UString::UTF_8);
        idManager_->getTermIdByTermString( queryTree->keywordUString_ , queryTree->keywordId_ );
        return !QueryUtility::isRestrictWord( queryTree->keywordUString_ );
    } // end - setKeyword()

} // end - namespace sf1r