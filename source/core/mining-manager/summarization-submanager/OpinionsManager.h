#ifndef SF1R_MINING_MANAGER_SUMMARIZATION_OPINIONMANAGER_H
#define SF1R_MINING_MANAGER_SUMMARIZATION_OPINIONMANAGER_H

#include <icma/icma.h>

#include "Ngram.h"
#include <vector>
#include <list>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <string>
#include <util/PriorityQueue.h>
#include <queue>
#include <bitset>

#define MAX_COMMENT_NUM  10000

using std::string;
using std::vector;

class cma::Analyzer;
class cma::Knowledge;

namespace sf1r
{

class OpinionsManager
{
public:

    struct CustomInt
    {
        CustomInt()
            :inner_(0)
        {
        }
        CustomInt(int in)
            :inner_(in)
        {
        }
        operator int&()
        {
            return inner_;
        }
        operator int() const
        {
            return inner_;
        }
        CustomInt& operator +=(int value)
        {
            inner_ += value;
            return *this;
        }
        int inner_;
    };
    // store single comment and split string.
    typedef std::vector< UString > OriginalCommentT;
    typedef std::vector< OriginalCommentT > OriginalCommentContainerT;
    typedef std::vector< UString > SentenceContainerT;
    typedef std::vector< UString > WordSegContainerT;
    typedef std::pair<UString, int> WordFreqPairT;
    // record how many sentences the word appeared in.
    typedef boost::unordered_map<UString, CustomInt> WordFreqMapT;

    typedef boost::unordered_map<UString, std::bitset<MAX_COMMENT_NUM> >  WordInSentenceMapT;
    typedef boost::unordered_map<UString, double> WordPossibilityMapT;
    typedef boost::unordered_map<UString, WordPossibilityMapT> WordJoinPossibilityMapT;

    typedef std::map<UString, double>  CachedStorageT;

    typedef std::pair< UString, UString > BigramPhraseT;
    typedef std::vector< BigramPhraseT > BigramPhraseContainerT;
    typedef std::vector< UString > NgramPhraseT;
    typedef std::vector< NgramPhraseT > NgramPhraseContainerT;
    typedef std::vector< NgramPhraseT > OpinionContainerT;
    typedef std::pair< NgramPhraseT, double > OpinionCandidateT;
    typedef std::vector< OpinionCandidateT > OpinionCandidateContainerT;

    OpinionsManager(const string& colPath, const std::string& dictpath);//string enc,string cate,string posDelimiter,
    ~OpinionsManager();
    void setComment(const SentenceContainerT& Z_);
    std::vector< std::pair<double, UString> > getOpinion(bool need_orig_comment_phrase = true);

    void setWindowsize(int C);
    void setSigma(double SigmaRep_,double SigmaRead_,double SigmaSim_,double SigmaLength_);
    void setEncoding(izenelib::util::UString::EncodingType encoding);
    void setFilterStr(const std::vector<UString>& filter_strs);
    void CleanCacheData();

private:
    void recompute_srep(std::vector< std::pair<double, UString> >& candList);
    void RefineCandidateNgram(OpinionCandidateContainerT& candList);
    void stringToWordVector(const UString& Mi, WordSegContainerT& words);
    void WordVectorToString(UString& Mi, const WordSegContainerT& words);
    double Sim(const UString& Mi, const UString& Mj);
    double Sim(const NgramPhraseT& wordsi, const NgramPhraseT& wordsj);
    double Srep(const NgramPhraseT& words);
    double SrepSentence(const UString& phrase_str);
    double Score(const NgramPhraseT& words);
    //rep
    double PMIlocal(const WordSegContainerT& words, const int& offset, int C=3);
    double PMImodified(const UString& Wi, const UString& Wj, int C=3);
    double Possib(const UString& Wi, const UString& Wj);
    double Possib(const UString& Wi);
    double CoOccurring(const UString& Wi, const UString& Wj,int C=3);
    bool CoOccurringInOneSentence(const UString& Wi,
        const UString& Wj, int C, const UString& sentence);
    //algorithm
    bool GenCandidateWord(WordSegContainerT& wordlist);
    bool GenSeedBigramList(BigramPhraseContainerT& result_list);
    bool GetFinalMicroOpinion(const BigramPhraseContainerT& seed_bigramlist,
       bool need_orig_comment_phrase, std::vector< std::pair<double, UString> >& final_result);
    void ValidCandidateAndUpdate(const NgramPhraseT& phrase, OpinionCandidateContainerT& candList);
    bool NotMirror(const WordSegContainerT& phrase, const BigramPhraseT& bigram);
    void Merge(NgramPhraseT& phrase, const BigramPhraseT& bigram);
    BigramPhraseContainerT GetJoinList(const WordSegContainerT& phrase, const BigramPhraseContainerT& BigramList, int current_merge_pos);
    void GenerateCandidates(const NgramPhraseT& phrase, OpinionCandidateContainerT& candList, 
        const BigramPhraseContainerT& seedBigrams, int current_merge_pos);
    void changeForm(const OpinionCandidateContainerT& candList, std::vector<std::pair<double, UString> >& newForm);
   
    std::string getSentence(const WordSegContainerT& candVector);
    bool IsNeedFilte(const UString& teststr);
    void GetOrigCommentsByBriefOpinion(std::vector< std::pair<double, UString> >& candOpinionString);
    bool FilterBigramByPossib(double possib, const OpinionsManager::BigramPhraseT& bigram);

private:
    class WordPriorityQueue_ : public izenelib::util::PriorityQueue<WordFreqPairT>
    {
    public:
        WordPriorityQueue_()
        {
        }
        void Init(size_t s)
        {
            initialize(s);
        }
    protected:
        bool lessThan(const WordFreqPairT& o1, const WordFreqPairT& o2) const
        {
            return (o1.second < o2.second);
        }
    };
    typedef std::priority_queue<double, std::vector<double>, greater<double> >  CandidateSrepQueueT;

    double SigmaRep;
    CandidateSrepQueueT SigmaRep_dynamic;
    double SigmaRead;
    double SigmaSim;
    double SigmaLength;
    SentenceContainerT Z;
    fstream out;
    int windowsize;
    CachedStorageT cached_srep;
    WordInSentenceMapT  cached_word_insentence_;
    izenelib::util::UString::EncodingType encodingType_;
    WordJoinPossibilityMapT  cached_pmimodified_;
    OriginalCommentContainerT orig_comments_;
    WordSegContainerT  filter_strs_;
    cma::Analyzer* analyzer_;
    cma::Knowledge* knowledge_;
    size_t word_cache_hit_num_;
    size_t pmi_cache_hit_num_;
};

};

#endif
