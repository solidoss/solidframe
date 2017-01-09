#include <iostream>
#include <sstream>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

using namespace std;

struct Base{
    virtual ~Base(){
    }
    virtual void print() const = 0;
};


struct A: Base{
    int     v;
    
    
    A(int _v = -1): v(_v){}
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        boost::serialization::void_cast_register<A, Base>(
            static_cast<A *>(NULL),
            static_cast<Base *>(NULL)
        );
        ar & v;
    }
    
    virtual void print() const {
        cout<<"A::v = "<<v<<endl;
    }
};

struct B: Base{
    string      v;
    
    B(string const &_v = ""):v(_v){}
    
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        boost::serialization::void_cast_register<B, Base>(
            static_cast<B *>(NULL),
            static_cast<Base *>(NULL)
        );
        ar & v;
    }
    
    virtual void print() const {
        cout<<"B::v = "<<v<<endl;
    }
};

struct C: B{
    int     v;
    
    C(int _v1 = -1, string const&_v2 = ""): B(_v2), v(_v1){}
    
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        boost::serialization::void_cast_register<C, Base>(
            static_cast<C *>(NULL),
            static_cast<Base *>(NULL)
        );
        boost::serialization::void_cast_register<C, B>(
            static_cast<C *>(NULL),
            static_cast<B *>(NULL)
        );
        ar & static_cast<B&>(*this);
        ar & v;
    }
    
    virtual void print() const {
        cout<<"C::";
        B::print();
        cout<<"C::v = "<<v<<endl;
    }
};



int main(){
    string      data;
    {
        ostringstream                   oss;
        boost::archive::text_oarchive   oa(oss);
        
        oa.template register_type<A>();
        oa.template register_type<B>();
        oa.template register_type<C>();
        
        Base                            *p1 = new A(1);
        Base                            *p2 = new B("ceva");
        Base                            *p3 = new C(2, "altceva");
        
        oa<<p1;
        oa<<p2;
        oa<<p3;
        data = oss.str();
    }
    cout<<"After serialization: {"<<endl<<data<<"}"<<endl;
    {
        istringstream                   iss(data);
        boost::archive::text_iarchive   ia(iss);
        
        
        ia.template register_type<A>();
        ia.template register_type<B>();
        ia.template register_type<C>();
        
        Base                            *p1 = nullptr;
        Base                            *p2 = nullptr;
        Base                            *p3 = nullptr;
        
        ia>>p1>>p2>>p3;
        
        p1->print();
        p2->print();
        p3->print();
    }
    return 0;
}