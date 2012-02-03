/* Declarations file dynamic.hpp
	
	Copyright 2010 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UTILITY_DYNAMIC_TYPE_HPP
#define UTILITY_DYNAMIC_TYPE_HPP

#include <vector>

#include "system/common.hpp"
#include "system/cassert.hpp"

#include "utility/dynamicpointer.hpp"
#include "utility/shared.hpp"

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
	static bool isType(uint32 _id){
		return false;
	}
	//! Get the type id for a Dynamic object.
	virtual uint32 dynamicTypeId()const = 0;
	//! Return the callback from the given DynamicMap associated to this object
	virtual DynamicMap::FncT callback(const DynamicMap &_rdm);
	//! Used by DynamicPointer - smartpointers
	virtual void use();
	//! Used by DynamicPointer to know if the object must be deleted
	/*!
	 * For the return value, think of use count. Returning zero means the object should
	 * be deleted. Returning non zero means the object should not be deleted.
	 */
	virtual int release();
	
	virtual bool isTypeDynamic(uint32 _id)const;

protected:
	friend class DynamicPointerBase;
	virtual ~DynamicBase();
};

struct DynamicSharedImpl: Shared{
protected:
	DynamicSharedImpl():usecount(0){}
	void doUse();
	int doRelease();
protected:
	int		usecount;
};

//! A class for 
template <class B = DynamicBase>
struct DynamicShared: B, DynamicSharedImpl{
	//! Used by DynamicPointer - smartpointers
	/*virtual*/ void use(){
		doUse();
	}
	//! Used by DynamicPointer to know if the object must be deleted
	/*virtual*/ int release(){
		return doRelease();
	}
	
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
	explicit Dynamic(G1 &_g1):T(_g1){}
	
	template<class G1>
	explicit Dynamic(const G1 &_g1):T(_g1){}
	
	//! Two parameters constructor to forward to base
	template<class G1, class G2>
	explicit Dynamic(G1 &_g1, G2 &_g2):T(_g1, _g2){}
	
	template<class G1, class G2>
	explicit Dynamic(const G1 &_g1, const G2 &_g2):T(_g1, _g2){}
	
	//! Three parameters constructor to forward to base
	template<class G1, class G2, class G3>
	explicit Dynamic(G1 &_g1, G2 &_g2, G3 &_g3):T(_g1, _g2, _g3){}
	
	template<class G1, class G2, class G3>
	explicit Dynamic(const G1 &_g1, const G2 &_g2, const G3 &_g3):T(_g1, _g2, _g3){}
	
	template<class G1, class G2, class G3, class G4>
	explicit Dynamic(G1 &_g1, G2 &_g2, G3 &_g3, G4 &_g4):T(_g1, _g2, _g3, _g4){}
	
	template<class G1, class G2, class G3, class G4>
	explicit Dynamic(const G1 &_g1, const G2 &_g2, const G3 &_g3, const G4 &_g4):T(_g1, _g2, _g3, _g4){}
	
	template<class G1, class G2, class G3, class G4, class G5>
	explicit Dynamic(G1 &_g1, G2 &_g2, G3 &_g3, G4 &_g4, G5 &_g5):T(_g1, _g2, _g3, _g4, _g5){}
	
	template<class G1, class G2, class G3, class G4, class G5>
	explicit Dynamic(const G1 &_g1, const G2 &_g2, const G3 &_g3, const G4 &_g4, const G5 &_g5):T(_g1, _g2, _g3, _g4, _g5){}
	
	template<class G1, class G2, class G3, class G4, class G5, class G6>
	explicit Dynamic(const G1 &_g1, const G2 &_g2, const G3 &_g3, const G4 &_g4, const G5 &_g5, const G6 &_g6):T(_g1, _g2, _g3, _g4, _g5, _g6){}
	
	template<class G1, class G2, class G3, class G4, class G5, class G6, class G7>
	explicit Dynamic(G1 &_g1, G2 &_g2, G3 &_g3, G4 &_g4, G5 &_g5, G6 &_g6, G7 &_g7):T(_g1, _g2, _g3, _g4, _g5, _g6, _g7){}
	
	template<class G1, class G2, class G3, class G4, class G5, class G6>
	explicit Dynamic(G1 &_g1, G2 &_g2, G3 &_g3, G4 &_g4, G5 &_g5, G6 &_g6):T(_g1, _g2, _g3, _g4, _g5, _g6){}
	
	template<class G1, class G2, class G3, class G4, class G5, class G6, class G7>
	explicit Dynamic(const G1 &_g1, const G2 &_g2, const G3 &_g3, const G4 &_g4, const G5 &_g5, const G6 &_g6, const G7 &_g7):T(_g1, _g2, _g3, _g4, _g5, _g6, _g7){}
	
	//!The static type id
#ifdef HAVE_SAFE_STATIC
	static uint32 staticTypeId(){
		static uint32 id(DynamicMap::generateId());
		return id;
	}
#else
private:
	static uint32 staticTypeIdStub(){
		static uint32 id(DynamicMap::generateId());
		return id;
	}
	static void once_cbk(){
		staticTypeIdStub();
	}
public:
	static uint32 staticTypeId(){
		static boost::once_flag once = BOOST_ONCE_INIT;
		boost::call_once(&once_cbk, once);
		return staticTypeIdStub();
	}
#endif
	//TODO: add:
	//static bool isTypeExplicit(const DynamicBase*);
	static bool isType(uint32 _id){
		if(_id == staticTypeId()) return true;
		return BaseT::isType(_id);
	}
	//! The dynamic typeid
	virtual uint32 dynamicTypeId()const{
		return staticTypeId();
	}
	virtual bool isTypeDynamic(uint32 _id)const{
		if(_id == staticTypeId()) return true;
		return BaseT::isTypeDynamic(_id);
	}
	//! Returns the associated callback from the given DynamicMap
	virtual DynamicMap::FncT callback(const DynamicMap &_rdm){
		DynamicMap::FncT pf = _rdm.callback(staticTypeId());
		if(pf) return pf;
		return T::callback(_rdm);
	}
	X* cast(DynamicBase *_pdb){
		if(isTypeDynamic(_pdb->dynamicTypeId())){
			return static_cast<X*>(_pdb);
		}
		return NULL;
	}
};


struct DynamicDefaultPointerStore{
protected:
	void pushBack(void *, const uint _idx, const DynamicPointer<DynamicBase> &_dp){
		v[_idx].push_back(_dp);
	}
	size_t size(void *, const uint _idx)const{
		return v[_idx].size();
	}
	bool isNotLast(void *, const uint _idx, const uint _pos)const{
		return _pos < v[_idx].size();
	}
	const DynamicPointer<DynamicBase>& pointer(void *, const uint _idx, const uint _pos)const{
		return v[_idx][_pos];
	}
	DynamicPointer<DynamicBase>& pointer(void *, const uint _idx, const uint _pos){
		return v[_idx][_pos];
	}
	void clear(void *, const uint _idx){
		v[_idx].clear();
	}
private:
	typedef std::vector<DynamicPointer<DynamicBase> > DynamicPointerVectorT;
	DynamicPointerVectorT	v[2];
};

//! A templated dynamic executer
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
template <class R, class Exe, class S = DynamicDefaultPointerStore, class P = void>
struct DynamicExecuter;

//! Specialization for DynamicExecuter with no extra parameter to dynamicExecute
template <class R, class Exe, class S>
struct DynamicExecuter<R, Exe , S, void>: public S{
private:
	typedef R (*FncT)(const DynamicPointer<DynamicBase> &, void*);
	
	template <class Q, class E>
	static R doExecute(const DynamicPointer<DynamicBase> &_dp, void *_e){
		E *pe = reinterpret_cast<E*>(_e);
		DynamicPointer<Q>	ds(_dp);
		return pe->dynamicExecute(ds);
	}
	
	template <class E>
	static DynamicMap& dynamicMapEx(){
		static DynamicMap	dm(dynamicMap());
		dynamicMap(&dm);
		return dm;
	}
public:
	//! Basic constructor
	DynamicExecuter():pushid(0), execid(1), crtpos(0){}
	
	//! Push a new object for later execution
	/*! 
		This method and prepareExecute should be synchronized.
		\param _dp a DynamicPointer to a DynamicBase object
	*/
	void push(Exe *_pexe, const DynamicPointer<DynamicBase> &_dp){
		this->pushBack(_pexe, pushid, _dp);
	}
	//! Move the objects from push queue to execute queue.
	uint prepareExecute(Exe *_pexe){
		this->clear(_pexe, execid);
		uint tmp = execid;
		execid = pushid;
		pushid = tmp;
		crtpos = 0;
		return this->size(_pexe, execid);
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
	inline bool hasCurrent(Exe *_pexe)const{
		return this->isNotLast(_pexe, execid, crtpos);
	}
	//! Iterate forward through execution queue
	inline void next(Exe *_pexe){
		cassert(hasCurrent(_pexe));
		this->pointer(_pexe, execid, crtpos).clear();
		++crtpos;
	}
	//! Execute the current object from execution queue
	/*!
		I.e., call the coresponding this->dynamicExecute(DynamicPointer\<RealObjectType>&).<br>
		If there is no dynamicExecute method registered for the current object,
		it will call _re.dynamicExecuteDefault(DynamicPointer\<> &).
		\param _re Reference to the executer, usually *this.
		\retval R the return value for dynamicExecute methods.
	*/
	R executeCurrent(Exe *_pexe){
		cassert(hasCurrent(_pexe));
		DynamicRegistererBase			dr;
		DynamicPointer<DynamicBase>		&rdp(this->pointer(_pexe, execid, crtpos));
		dr.lock();
		FncT	pf = reinterpret_cast<FncT>(rdp->callback(*dynamicMap()));
		dr.unlock();
		if(pf) return (*pf)(rdp, _pexe);
		return _pexe->dynamicExecute(rdp);
	}
	
	void executeAll(Exe *_pexe){
		while(hasCurrent(_pexe)){
			executeCurrent(_pexe);
			next(_pexe);
		}
	}
	
	R execute(Exe *_pexe, DynamicPointer<DynamicBase> &_rdp){
		DynamicRegistererBase	dr;
		dr.lock();
		FncT	pf = reinterpret_cast<FncT>(_rdp->callback(*dynamicMap()));
		dr.unlock();
		if(pf) return (*pf)(_rdp, _pexe);
		return _pexe->dynamicExecute(_rdp);
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
		DynamicReceiverT::registerDynamic\<ASignal, ExeOne>();<br>
		DynamicReceiverT::registerDynamic\<BSignal, ExeOne>();<br>
		</code>
	*/
	template <class Q, class E>
	static void registerDynamic(){
		FncT	pf = &doExecute<Q, E>;
		dynamicMapEx<E>().callback(Q::staticTypeId(), reinterpret_cast<DynamicMap::FncT>(pf));
	}
	
private:
	uint				pushid;
	uint				execid;
	uint				crtpos;
};


//! DynamicExecuter with an extra parameter to dynamicExecute
template <class R, class Exe, class S, class P>
struct DynamicExecuter: public S{
private:
	typedef R (*FncT)(const DynamicPointer<DynamicBase> &, void*, P);
	
	template <class Q, class E>
	static R doExecute(const DynamicPointer<DynamicBase> &_dp, void *_e, P _p){
		E *pe = reinterpret_cast<E*>(_e);
		DynamicPointer<Q>	ds(_dp);
		return pe->dynamicExecute(ds, _p);
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
	DynamicExecuter():pushid(0), execid(1), crtpos(0){}
	
	//! Push a new object for later execution
	/*! 
		This method and prepareExecute should be synchronized.
		\param _dp a DynamicPointer to a DynamicBase object
	*/
	void push(Exe *_pexe, const DynamicPointer<DynamicBase> &_dp){
		this->pushBack(_pexe, pushid, _dp);
	}
	//! Move the objects from push queue to execute queue.
	uint prepareExecute(Exe *_pexe){
		this->clear(_pexe, execid);
		uint tmp = execid;
		execid = pushid;
		pushid = tmp;
		crtpos = 0;
		return this->size(_pexe, execid);
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
	inline bool hasCurrent(Exe *_pexe)const{
		return this->isNotLast(_pexe, execid, crtpos);
	}
	//! Iterate forward through execution queue
	inline void next(Exe *_pexe){
		cassert(hasCurrent(_pexe));
		this->pointer(_pexe, execid, crtpos).clear();
		++crtpos;
	}
	//! Execute the current object from execution queue
	/*!
		I.e., call the coresponding this->dynamicExecute(DynamicPointer\<RealObjectType>&).<br>
		If there is no dynamicExecute method registered for the current object,
		it will call _re.dynamicExecuteDefault(DynamicPointer\<> &).
		\param _re Reference to the executer, usually *this.
		\retval R the return value for dynamicExecute methods.
	*/
	R executeCurrent(Exe *_pexe, P _p){
		cassert(hasCurrent(_pexe));
		DynamicRegistererBase			dr;
		DynamicPointer<DynamicBase>		&rdp(this->pointer(_pexe, execid, crtpos));
		dr.lock();
		FncT	pf = reinterpret_cast<FncT>(rdp->callback(*dynamicMap()));
		dr.unlock();
		if(pf) return (*pf)(rdp, _pexe, _p);
		return _pexe->dynamicExecute(rdp, _p);
	}
	
	void executeAll(Exe *_pexe, P _p){
		while(hasCurrent(_pexe)){
			executeCurrent(_pexe, _p);
			next(_pexe);
		}
	}
	
	R execute(Exe &_re, DynamicPointer<> &_rdp, P _p){
		DynamicRegistererBase	dr;
		dr.lock();
		FncT	pf = reinterpret_cast<FncT>(_rdp->callback(*dynamicMap()));
		dr.unlock();
		if(pf) return (*pf)(_rdp, &_re, _p);
		
		return _re.dynamicExecute(_rdp, _p);
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
		DynamicReceiverT::registerDynamic\<ASignal, ExeOne>();<br>
		DynamicReceiverT::registerDynamic\<BSignal, ExeOne>();<br>
		</code>
	*/
	template <class Q, class E>
	static void registerDynamic(){
		FncT	pf = &doExecute<Q, E>;
		dynamicMapEx<E>().callback(Q::staticTypeId(), reinterpret_cast<DynamicMap::FncT>(pf));
	}
	
private:
	uint				pushid;
	uint				execid;
	uint				crtpos;
};


#endif
