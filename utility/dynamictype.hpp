#ifndef SYSTEM_DYNAMIC_HPP
#define SYSTEM_DYNAMIC_HPP

#include "system/common.hpp"
#include "system/cassert.hpp"
#include <vector>
#include "utility/dynamicpointer.hpp"
//! Store a map from a typeid to a callback
/*!
	The type id is determined using Dynamic::dynamicTypeId() or Dynamic::staticTypeId().
	Use callback(uint32, FncT) to register a pair of typeid and callback.
	Use FncT callback(uint32) to retrieve a registered callback.
*/
struct DynamicMap{
	typedef void (*FncT)(const DynamicPointer<DynamicBase> &,void*);
	typedef void (*RegisterFncT)(DynamicMap &_rdm);
	static uint32 generateId();
	//!Constructor
	/*!
		\param _pdm if not NULL, the constructor act like a copy constructor.<br>
		This is used for mapping callbacks from inheritant DynamicReceiver classes.<br>
		If DynamicReceiverB is a DynamicReceiverA, it means that all callbacks
		from the DynamicMap associated to DynamicReceiverA are also valid for
		the DynamicMap associated to DynamicReceiverB.<br>
		
	*/
	DynamicMap(DynamicMap *_pdm);
	~DynamicMap();
	//! Register a callback
	/*!
		\param _tid the type id as returned by Dynamic::staticTypeId
		\param _pf a pointer to a static function
	*/
	void callback(uint32 _tid, FncT _pf);
	//! Retrieve an already registered callback.
	/*!
		\param _id The typeid as returned by Dynamic::dynamicTypeId
		\retval FncT the callback for typeid or NULL.
	*/
	FncT callback(uint32 _id)const;
	struct Data;
	Data	&d;
};

//! A base class for all DynamicRegisterers
struct DynamicRegistererBase{
	void lock();
	void unlock();
};

//! A dynamic registerer
/*!
	Use it like this:<br>
	In your receiver class define a static YourReceiver::dynamicRegister function.<br>
	Define a static object: static DynamicRegisterer\<YourReceiver>	dr.<br>
	Note, that if your Receiver inherits from OtherReceiver, first thing you 
	must do in YourReceiver::dynamicRegister, is to call OtherReceiver::dynamicRegister.
*/
template <class T>
struct DynamicRegisterer: DynamicRegistererBase{
	DynamicRegisterer(){
		lock();
		T::dynamicRegister();
		unlock();
	}
};

//struct DynamicPointerBase;
//! A base for all types that needs dynamic typeid.
struct DynamicBase{
	//! Get the type id for a Dynamic object.
	virtual uint32 dynamicTypeId()const = 0;
	//! Return the callback from the given DynamicMap associated to this object
	virtual DynamicMap::FncT callback(const DynamicMap &_rdm);
	//! Used by DynamicPointer - smartpointers
	virtual void use();
	//! Used by DynamicPointer to know if the object must be deleted
	virtual int release();

protected:
	friend struct DynamicPointerBase;
	virtual ~DynamicBase();
};

//! Template class to provide dynamic type functionality
/*!
	If you need to have dynamic type functionality for your objects,
	you need to inherit them through Dynamic. Here's an example:<br>
	if you need to have:<br>
		C: public B <br>
		B: public A <br>
	you will actually have:<br> 
		C: public Dynamic\<C,B><br>
		B: public Dynamic\<B,A><br>
		A: public Dynamic\<A>
*/
template <class X, class T = DynamicBase>
struct Dynamic: T{
	typedef Dynamic<X,T>	BaseT;
	
	//! Basic constructor
	Dynamic(){}
	
	//! One parameter constructor to forward to base
	template<class G1>
	Dynamic(G1 _g1):T(_g1){}
	
	//! Two parameters constructor to forward to base
	template<class G1, class G2>
	Dynamic(G1 _g1, G2 _g2):T(_g1, _g2){}
	
	//! Three parameters constructor to forward to base
	template<class G1, class G2, class G3>
	Dynamic(G1 _g1, G2 _g2, G3 _g3):T(_g1, _g2, _g3){}
	
	//!The static type id
	static uint32 staticTypeId(){
		//TODO: staticproblem
		static uint32 id(DynamicMap::generateId());
		return id;
	}
	//TODO: add:
	//static bool isTypeExplicit(const DynamicBase*);
	//static bool isType(const DynamicBase*);
	//! The dynamic typeid
	virtual uint32 dynamicTypeId()const{
		return staticTypeId();
	}
	//! Returns the associated callback from the given DynamicMap
	virtual DynamicMap::FncT callback(const DynamicMap &_rdm){
		DynamicMap::FncT pf = _rdm.callback(staticTypeId());
		if(pf) return pf;
		return T::callback(_rdm);
	}
};

//! A templated dynamic receiver
/*!
	The ideea of this class is to ease the following process:<br>
	You want to signal an object with different types of signals.<br>
	You want to easely add support for executing certain signals.<br>
	Also you want that your Executers are inheritable. That is you want:<br>
	ExeOne know about ASignal, BSignal.<br>
	ExeTwo inherits from ExeOne and knows about ASignal, BSignal, CSignal, 
	but only adds support for CSignal as the other signals are known by
	ExeOne.
	You need only one DynamicReceiver for an inheritance branch.<br>
	In the above case, you only need a DynamicReciever\<void, ExeOne>
	as protected member of ExeOne.<br><br>
	
	The first template member is the return value of methods associated
	to a signal, in our case:<br>
	void ExeOne::dynamicExecute(DynamicPointer\<ASignal> &);<br>
	void ExeOne::dynamicExecute(DynamicPointer\<BSignal> &);<br>
	void ExeTwo::dynamicExecute(DynamicPointer\<CSignal> &);<br>
	For a full example see: examples/utility/dynamictest/dynamictest.cpp.<br>
	<br>
	Although it does not offer support for synchronization,
	it is prepare for synchronized usage.
	Basically DynamicReceiver offers the following support:<br>
	On one end one can synchronized push DynamicBase objects.<br>
	At other end, one can synchrononized move all objects from
	push queue to exe queue. Then without synchronization one should,
	prepare for execution (prepareExecute), the iterate throuh
	received objects and execute them: hasCurrent, next, executeCurrent
	
	
*/
template <class R, class Exe>
struct DynamicReceiver{
private:
	typedef R (*FncT)(const DynamicPointer<DynamicBase> &, void*);
	
	template <class S, class E>
	static R doExecute(const DynamicPointer<DynamicBase> &_dp, void *_e){
		E *pe = reinterpret_cast<E*>(_e);
		DynamicPointer<S>	ds(_dp);
		return pe->dynamicExecute(ds);
	}
	
	typedef std::vector<DynamicPointer<DynamicBase> > DynamicPointerVectorT;
	template <class E>
	static DynamicMap& dynamicMapEx(){
		static DynamicMap	dm(dynamicMap());
		dynamicMap(&dm);
		return dm;
	}
public:
	//! Basic constructor
	DynamicReceiver():pushid(0), execid(1){}
	
	//! Push a new object for later execution
	/*! 
		This method and prepareExecute should be synchronized.
		\param _dp a DynamicPointer to a DynamicBase object
	*/
	void push(const DynamicPointer<DynamicBase> &_dp){
		v[pushid].push_back(_dp);
	}
	//! Move the objects from push queue to execute queue.
	uint prepareExecute(){
		v[execid].clear();
		uint tmp = execid;
		execid = pushid;
		pushid = tmp;
		crtit = v[execid].begin();
		return v[execid].size();
	}
	
	//! Use this method to iterate through the objects from the execution queue
	/*!
		<code>
		lock();<br>
		dr.prepareExecute();<br>
		unlock();<br>
		while(dr.hasCurrent()){<br>
			dr.executeCurrent(*this);<br>
			dr.next();<br>
		}<br>
		</code>
	*/
	inline bool hasCurrent()const{
		return crtit != v[execid].end();
	}
	//! Iterate forward through execution queue
	inline void next(){
		cassert(crtit != v[execid].end());
		crtit->clear();
		++crtit;
	}
	//! Execute the current object from execution queue
	/*!
		I.e., call the coresponding this->dynamicExecute(DynamicPointer\<RealObjectType>&).<br>
		If there is no dynamicExecute method registered for the current object,
		it will call _re.dynamicExecuteDefault(DynamicPointer\<> &).
		\param _re Reference to the executer, usually *this.
		\retval R the return value for dynamicExecute methods.
	*/
	template <typename E>
	R executeCurrent(E &_re){
		cassert(crtit != v[execid].end());
		FncT	pf = reinterpret_cast<FncT>((*crtit)->callback(*dynamicMap()));
		if(pf) return (*pf)(*crtit, &_re);
		return _re.dynamicExecuteDefault(*crtit);
	}
	
	static DynamicMap* dynamicMap(DynamicMap *_pdm = NULL){
		static DynamicMap *pdm(_pdm);
		if(_pdm) pdm = _pdm;
		return pdm;
	}
	
	//! Register a new dynamicExecute method
	/*!
		Usage:<br>
		<code>
		DynamicReceiverT::add\<ASignal, ExeOne>();<br>
		DynamicReceiverT::add\<BSignal, ExeOne>();<br>
		</code>
	*/
	template <class S, class E>
	static void add(){
		FncT	pf = &doExecute<S, E>;
		dynamicMapEx<E>().callback(S::staticTypeId(), reinterpret_cast<DynamicMap::FncT>(pf));
	}
	
private:
	DynamicPointerVectorT						v[2];
	DynamicPointerVectorT::iterator			crtit;
	uint										pushid;
	uint										execid;
};


#endif
