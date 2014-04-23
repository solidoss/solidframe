#include <streambuf>
#include <istream>
#include <ostream>
#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include "system/debug.hpp"
#include "system/filedevice.hpp"
#include "system/cassert.hpp"

#include "frame/file/filestream.hpp"

using namespace std;
using namespace solid;

int main(int argc, char *argv[]){
	
	Debug::the().levelMask("view");
	Debug::the().moduleMask("any");
	Debug::the().initStdErr(false);
	
	idbg("FileBuf example");
	
	if(0){
		
		frame::file::File				fd;
		frame::file::FilePointerT		ptr(&fd);
		if(!fd.open("test.txt", FileDevice::WriteOnlyE | FileDevice::CreateE | FileDevice::TruncateE)){
			idbg("Could not open file for writing");
			return 0;
		}
		frame::file::FileOStream<0>		ofs(ptr, 0);
		
		ofs<<'['<<' '<<"this is the most interesting text that was ever written to a file "<<123456789<<' '<<']'<<endl;
	}
	if(0){
		frame::file::File				fd;
		frame::file::FilePointerT		ptr(&fd);
		
		if(!fd.open("test.txt", FileDevice::ReadWriteE)){
			idbg("Could not open file for read-write");
			return 0;
		}
		frame::file::FileIOStream<8>	iofs/*(0, 0)*/;
		iofs.device(ptr);
		iofs.seekp(0, ios_base::end);
		iofs<<'<'<<' '<<"this is another line way more interesting than the above one "<<9876543210<<' '<<'>'<<endl;
		string				line;
		getline(iofs, line);
		cout<<"Read first line: "<<line<<endl;
		getline(iofs, line);
		cout<<"Read second line: "<<line<<endl;
		line += '\n';
		iofs.write(line.data(), line.size()).flush();
		iofs.seekg(100);
		iofs.seekp(100);
	}
	if(0){
		frame::file::File				fd;
		frame::file::FilePointerT		ptr(&fd);
		if(!fd.open("test.txt", FileDevice::ReadOnlyE)){
			idbg("Could not open file for read");
			return 0;
		}
		frame::file::FileIStream<8>		ifs;
		ifs.device(ptr);
		string				line;
		getline(ifs, line);
		cout<<"Read first line: "<<line<<endl;
		getline(ifs, line);
		cout<<"Read second line: "<<line<<endl;
		getline(ifs, line);
		cout<<"Read third line: "<<line<<endl;
		ifs.seekg(10);
		cout<<"seek 10 get char: "<<(int)ifs.peek()<<endl;
		ifs.seekg(12);
		cout<<"seek 12 get char: "<<(int)ifs.peek()<<endl;
	}
	if(1){
		
		fstream	fs;
		fs.open("test_1.txt", ios::out);
		fs.close();
		fs.open("test_1.txt");
		fs.seekp(10);
		fs<<"this is the most interesting text that was ever written to a file\n";
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		
		fs.seekg(0);
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		char c;
		fs>>c;
		cout<<"c = "<<(int)c<<endl;
		
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		
		fs.seekp(10);
		fs<<"this is another line way more interesting than the above one\n";
		
		
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		
		fs>>c;
		cout<<"c = "<<c<<endl;
		
		char buf[128];
		fs.seekg(8);
		
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		
		fs.read(buf, 8);
		cout<<"Read("<<fs.gcount()<<"): ";
		cout.write(buf, fs.gcount());
		cout<<endl;
		cout<<"Done STD"<<endl;
	}
	if(1){
		frame::file::File				fd;
		frame::file::FilePointerT		ptr(&fd);
		if(!fd.open("test_2.txt", FileDevice::ReadWriteE | FileDevice::CreateE | FileDevice::TruncateE)){
			idbg("Could not open file for read-write");
			return 0;
		}
		frame::file::FileIOStream<1024>	fs/*(0, 0)*/;
		fs.device(ptr);
		
		fs.seekp(10);
		fs<<"this is the most interesting text that was ever written to a file\n";
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		
		fs.seekg(0);
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		char c;
		fs>>c;
		cout<<"c = "<<(int)c<<endl;
		
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		
		fs.seekp(10);
		fs<<"this is another line way more interesting than the above one\n";
		
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		
		fs>>c;
		cout<<"c = "<<c<<endl;
		
		char buf[128];
		fs.seekg(8);
		
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		fs.read(buf, 8);
		cout<<"Read("<<fs.gcount()<<"): ";
		cout.write(buf, fs.gcount());
		cout<<endl;
	}
	return 0;
}