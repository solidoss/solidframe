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

namespace solid{

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
template <class T = DynamicBase>
struct DynamicShared: T, DynamicSharedImpl{
    DynamicShared(){}
	//! One parameter constructor to forward to base
	template<class G1>
	explicit DynamicShared(G1 &_g1):T(_g1){}
	
	template<class G1>
	explicit DynamicShared(const G1 &_g1):T(_g1){}
	
	//! Two parameters constructor to forward to base
	template<class G1, class G2>
	explicit DynamicShared(G1 &_g1, G2 &_g2):T(_g1, _g2){}
	
	template<class G1, class G2>
	explicit DynamicShared(const G1 &_g1, const G2 &_g2):T(_g1, _g2){}
	
	//! Three parameters constructor to forward to base
	template<class G1, class G2, class G3>
	explicit DynamicShared(G1 &_g1, G2 &_g2, G3 &_g3):T(_g1, _g2, _g3){}
	
	template<class G1, class G2, class G3>
	explicit DynamicShared(const G1 &_g1, const G2 &_g2, const G3 &_g3):T(_g1, _g2, _g3){}
	
	template<class G1, class G2, class G3, class G4>
	explicit DynamicShared(G1 &_g1, G2 &_g2, G3 &_g3, G4 &_g4):T(_g1, _g2, _g3, _g4){}
	
	template<class G1, class G2, class G3, class G4>
	explicit DynamicShared(const G1 &_g1, const G2 &_g2, const G3 &_g3, const G4 &_g4):T(_g1, _g2, _g3, _g4){}
	
	template<class G1, class G2, class G3, class G4, class G5>
	explicit DynamicShared(G1 &_g1, G2 &_g2, G3 &_g3, G4 &_g4, G5 &_g5):T(_g1, _g2, _g3, _g4, _g5){}
	
	template<class G1, class G2, class G3, class G4, class G5>
	explicit DynamicShared(const G1 &_g1, const G2 &_g2, const G3 &_g3, const G4 &_g4, const G5 &_g5):T(_g1, _g2, _g3, _g4, _g5){}
	
	template<class G1, class G2, class G3, class G4, class G5, class G6>
	explicit DynamicShared(const G1 &_g1, const G2 &_g2, const G3 &_g3, const G4 &_g4, const G5 &_g5, const G6 &_g6):T(_g1, _g2, _g3, _g4, _g5, _g6){}
	
	template<class G1, class G2, class G3, class G4, class G5, class G6, class G7>
	explicit DynamicShared(G1 &_g1, G2 &_g2, G3 &_g3, G4 &_g4, G5 &_g5, G6 &_g6, G7 &_g7):T(_g1, _g2, _g3, _g4, _g5, _g6, _g7){}
	
	template<class G1, class G2, class G3, class G4, class G5, class G6>
	explicit DynamicShared(G1 &_g1, G2 &_g2, G3 &_g3, G4 &_g4, G5 &_g5, G6 &_g6):T(_g1, _g2, _g3, _g4, _g5, _g6){}
	
	template<class G1, class G2, class G3, class G4, class G5, class G6, class G7>
	explicit DynamicShared(const G1 &_g1, const G2 &_g2, const G3 &_g3, const G4 &_g4, const G5 &_g5, const G6 &_g6, const G7 &_g7):T(_g1, _g2, _g3, _g4, _g5, _g6, _g7){}
	
	
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
#ifdef HAS_SAFE_STATIC
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

//! A templated dynamic handler
/*!
*/
template <class R, class O, class S = DynamicDefaultPointerStore, class P = void>
struct DynamicHandler;

//! Specialization for DynamicHandler with no extra parameter to dynamicHandle
template <class R, class O, class S>
struct DynamicHandler<R, O , S, void>: public S{
private:
	typedef R (*FncT)(const DynamicPointer<DynamicBase> &, void*);
	
	template <class D, class Obj>
	static R doHandle(const DynamicPointer<DynamicBase> &_rdynptr, void *_pobj){
		Obj &robj = *reinterpret_cast<Obj*>(_pobj);
		DynamicPointer<D>	dynptr(_rdynptr);
		return robj.dynamicHandle(dynptr);
	}
	
	template <class Obj>
	static DynamicMap& dynamicMapEx(){
		static DynamicMap	dm(dynamicMap());
		dynamicMap(&dm);
		return dm;
	}
public:
	//! Basic constructor
	DynamicHandler():pushid(0), objid(1), crtpos(0){}
	
	void push(O &_robj, const DynamicPointer<DynamicBase> &_rdynptr){
		this->pushBack(&_robj, pushid, _rdynptr);
	}

	uint prepareHandle(O &_robj){
		this->clear(&_robj, objid);
		uint tmp = objid;
		objid = pushid;
		pushid = tmp;
		crtpos = 0;
		return this->size(&_robj, objid);
	}
	
	inline bool hasCurrent(O &_robj)const{
		return this->isNotLast(&_robj, objid, crtpos);
	}
	
	inline void next(O &_robj){
		cassert(hasCurrent(_robj));
		this->pointer(&_robj, objid, crtpos).clear();
		++crtpos;
	}
	
	R handleCurrent(O &_robj){
		cassert(hasCurrent(_robj));
		DynamicRegistererBase			dr;
		DynamicPointer<DynamicBase>		&rdp(this->pointer(&_robj, objid, crtpos));
		dr.lock();
		FncT	pf = reinterpret_cast<FncT>(rdp->callback(*dynamicMap()));
		dr.unlock();
		if(pf) return (*pf)(rdp, &_robj);
		return _robj.dynamicHandle(rdp);
	}
	
	void handleAll(O &_robj){
		while(hasCurrent(_robj)){
			handleCurrent(_robj);
			next(_robj);
		}
	}
	
	R handle(O &_robj, DynamicPointer<DynamicBase> &_rdp){
		DynamicRegistererBase	dr;
		dr.lock();
		FncT	pf = reinterpret_cast<FncT>(_rdp->callback(*dynamicMap()));
		dr.unlock();
		if(pf) return (*pf)(_rdp, &_robj);
		return _robj.dynamicHandle(_rdp);
	}
	
	static DynamicMap* dynamicMap(DynamicMap *_pdm = NULL){
		static DynamicMap *pdm(_pdm);
		if(_pdm) pdm = _pdm;
		return pdm;
	}
	
	template <class D, class Obj>
	static void registerDynamic(){
		FncT	pf = &doHandle<D, Obj>;
		dynamicMapEx<Obj>().callback(D::staticTypeId(), reinterpret_cast<DynamicMap::FncT>(pf));
	}
	
private:
	uint				pushid;
	uint				objid;
	uint				crtpos;
};


//! DynamicHandler with an extra parameter to dynamicHandle
template <class R, class O, class S, class P>
struct DynamicHandler: public S{
private:
	typedef R (*FncT)(const DynamicPointer<DynamicBase> &, void*, P);
	
	template <class D, class Obj>
	static R doHandle(const DynamicPointer<DynamicBase> &_rdynptr, void *_pobj, P _p){
		Obj &robj = *reinterpret_cast<Obj*>(_pobj);
		DynamicPointer<D>	dynptr(_rdynptr);
		return robj.dynamicHandle(dynptr, _p);
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
	DynamicHandler():pushid(0), objid(1), crtpos(0){}
	
	void push(O &_robj, const DynamicPointer<DynamicBase> &_dynptr){
		this->pushBack(&_robj, pushid, _dynptr);
	}
	
	uint prepareHandle(O &_robj){
		this->clear(&_robj, objid);
		uint tmp = objid;
		objid = pushid;
		pushid = tmp;
		crtpos = 0;
		return this->size(&_robj, objid);
	}
	
	inline bool hasCurrent(O &_robj)const{
		return this->isNotLast(&_robj, objid, crtpos);
	}
	
	inline void next(O &_robj){
		cassert(hasCurrent(_robj));
		this->pointer(&_robj, objid, crtpos).clear();
		++crtpos;
	}
	
	R handleCurrent(O &_robj, P _p){
		cassert(hasCurrent(_robj));
		DynamicRegistererBase			dr;
		DynamicPointer<DynamicBase>		&rdp(this->pointer(&_robj, objid, crtpos));
		dr.lock();
		FncT	pf = reinterpret_cast<FncT>(rdp->callback(*dynamicMap()));
		dr.unlock();
		if(pf) return (*pf)(rdp, &_robj, _p);
		return _robj.dynamicHandle(rdp, _p);
	}
	
	void handleAll(O &_robj, P _p){
		while(hasCurrent(_robj)){
			handleCurrent(_robj, _p);
			next(_robj);
		}
	}
	
	R handle(O &_robj, DynamicPointer<> &_rdp, P _p){
		DynamicRegistererBase	dr;
		dr.lock();
		FncT	pf = reinterpret_cast<FncT>(_rdp->callback(*dynamicMap()));
		dr.unlock();
		if(pf) return (*pf)(_rdp, &_robj, _p);
		
		return _robj.dynamicHandle(_rdp, _p);
	}
	
	static DynamicMap* dynamicMap(DynamicMap *_pdm = NULL){
		static DynamicMap *pdm(_pdm);
		if(_pdm) pdm = _pdm;
		return pdm;
	}
	
	template <class D, class Obj>
	static void registerDynamic(){
		FncT	pf = &doHandle<D, Obj>;
		dynamicMapEx<Obj>().callback(D::staticTypeId(), reinterpret_cast<DynamicMap::FncT>(pf));
	}
	
private:
	uint				pushid;
	uint				objid;
	uint				crtpos;
};

}//namespace solid

#endif
