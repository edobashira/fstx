//Implementation of the Mohri's disambiguation algorithm
//for un-weighted acceptors

#include <fst/compose.h>
#include <fst/connect.h>
#include <fst/queue.h>
#include <fst/vector-fst.h>


namespace fst {
template<class Arc, class Queue>
class FsaDisambiguate {
 typedef typename Arc::StateId StateId;
 typedef typename Arc::Label Label;
 typedef typename Arc::Weight Weight;
 typedef pair<StateId, StateId> Pair;
 typedef basic_string<Label> String;
 typedef unordered_map<Label, const String *> LabelMap;

 class StringEqual {
   public:
    bool operator()(const String * const &x, const String * const &y) const {
      if (x->size() != y->size()) return false;
      for (size_t i = 0; i < x->size(); ++i)
        if ((*x)[i] != (*y)[i]) return false;
      return true;
    }
  };

  // Hash function for set of strings
  class StringKey{
   public:
    size_t operator()(const String * const & x) const {
      size_t key = x->size();
      for (size_t i = 0; i < x->size(); ++i)
        key = (key << 1) ^ (*x)[i];
      return key;
    }
  };

 struct Element {
   StateId state;
   const String* subset;
 };


 public:
  FsaDisambiguate(const Fst<Arc> &ifst, MutableFst<Arc> *ofst) 
    : ifst_(ifst.Copy()), ofst_(ofst)  {
    //Check the properties
    //Potentially sport
  }

  ~FsaDisambiguate() {
    for (typename StringSet::iterator it = string_set_.begin(); 
        it != string_set_.end(); ++it)
      delete *it;
  }

  void Expand(const Element &e, LabelMap *map) {
    for (typename String::const_iterator it = e.subset->begin(); 
        it != e.subset->end(); ++it) {
      for (ArcIterator<Fst<Arc> > aiter(*ifst_, *it); !aiter.Done();
          aiter.Next()) {
        const Arc &arc = aiter.Value();
      }
    }
  }

  void PrintElements() {
  }

  void PrintStrings() {
  }

  bool Disambiguate(SymbolTable *statelabels) {
    //Perform a self composition to get the
    //common future information
    CommonFutures();
    //Initial state, slightly easier than the Mohri paper
    //as there is only a single start state

    //Main traversal and disambiguation
    Queue q;

    String *si = new String;
    si->push_back(ifst_->Start());
    Element e = { ifst_->Start(), FindString(si) };
    StateId s = FindState(e);
    ofst_->SetStart(s);
    q.Enqueue(s);
    while (q.Empty()) {
      StateId s = q.Head();
      q.Dequeue();
      Element e = elements_[s];

      if (ifst_->Final(e.state) != Weight::Zero())
        ofst_->SetFinal(s, ifst_->Final(e.state));

      LabelMap label_map;
      Expand(e, &label_map);
      for (ArcIterator<Fst<Arc> > aiter(*ifst_, e.state); !aiter.Done(); 
           aiter.Next()) {
        const Arc &arc = aiter.Value();
      }
    }
    //Perform connection to remove dead-end states
    return true;
  }

 private:

  //Here manually pass in Composition table
  //this allows us to acess the state pair
  //We don't do a composition and trim because OpenFst will renumber the states
  //and this will require renumbering the state table, instead do a SCC and
  //get the coaccess states
  void CommonFutures() {
    typedef Matcher< Fst<Arc> > M;
    typedef SequenceComposeFilter<M> F;
    typedef GenericComposeStateTable<Arc, typename F::FilterState> T;
    typedef typename T::StateTuple ST;
    typedef ComposeFstOptions<Arc, M, F, T> Opts;
    Opts opts;
    opts.state_table = new T(*ifst_, *ifst_);
    ComposeFst<Arc> cfst(*ifst_, *ifst_, opts);
    //Use the conection code to find dead end states
    vector<bool> access;
    vector<bool> coaccess;
    uint64 props = 0;
    SccVisitor<Arc> scc_visitor(0, &access, &coaccess, &props);
    DfsVisit(cfst, &scc_visitor);
    //The coccess will tell us the state pair is reachable from the final
    //state and if the states share a common future
    const T &table = *opts.state_table;
    for (StateIterator<Fst<Arc> > siter(cfst); !siter.Done(); siter.Next()) {
      StateId s = siter.Value();
      const ST st = table.Tuple(s);
      Pair p(st.state_id1, st.state_id2); 
      state2pair_.push_back(p);
      pair2state_[p] = siter.Value();   
      if (coaccess[s]) {
        const ST st = table.Tuple(s);
        Pair p(st.state_id1, st.state_id2);
        common_futures_.insert(p);
      }
    }
  }

  static const size_t kPrime = 7853;

  struct PairHash {
    size_t operator()(const Pair &p) const {
      return p.first + p.second * kPrime;
    }
  };

  // Finds the string pointed by s in the hash set. Transfers the
  // pointer ownership to the hash set.
  const String *FindString(const String *s) {
    typename StringSet::iterator it = string_set_.find(s);
    if (it != string_set_.end()) {
      delete s;
      return (*it);
    } else {
      string_set_.insert(s);
      return s;
    }
  }

  StateId FindState(const Element &e) {
    typename ElementMap::iterator eit = element_map_.find(e);
    if (eit != element_map_.end()) {
      return (*eit).second;
    } else {
      StateId s = elements_.size();
      elements_.push_back(e);
      element_map_.insert(pair<const Element, StateId>(e, s));
      ofst_->AddState();
      return s;
    }
  }

  class ElementEqual {
   public:
    bool operator()(const Element &x, const Element &y) const {
      return x.state == y.state &&
              x.subset == y.subset;
    }
  };

  // Hash function for Elements to Fst states.
  class ElementKey {
   public:
    size_t operator()(const Element &x) const {
      size_t key = x.state;
      key = (key << 1) ^ (x.subset)->size();
      for (size_t i = 0; i < (x.subset)->size(); ++i)
        key = (key << 1) ^ (*x.subset)[i];
      return key;
    }
  };


  typedef unordered_map<Element, StateId, ElementKey, ElementEqual> ElementMap;
  typedef unordered_set<const String*, StringKey, StringEqual> StringSet;
  unordered_set<Pair, PairHash> common_futures_;
  vector<Pair> state2pair_;
  unordered_map<Pair, StateId, PairHash> pair2state_;
  StringSet string_set_;
  ElementMap element_map_;
  vector<Element> elements_;  // mapping Fst state to Elements
  const Fst<Arc> *ifst_;
  MutableFst<Arc> *ofst_;
};

template<class Arc>
bool Disambiguate(const Fst<Arc> &ifst, MutableFst<Arc> *ofst) {
  //Test the machine is an unweighted acceptor and is epsilon free
  FsaDisambiguate<Arc, FifoQueue<int> > fd(ifst, ofst);
  fd.Disambiguate(0);
  return true;
}

} //namespace fst

using namespace fst;
int main(int argc, char** argv) {
  StdFst *fst = StdFst::Read(argv[1]);
  StdVectorFst ofst;
  Disambiguate(*fst, &ofst);
  return 0;
}


