 #include <iostream>
#include <vector>
#include <tuple>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <algorithm>    // std::max
#include <iostream>
#include <fstream>
#include "locale"
#include "gmpfse.h"
#include <fstream>
#include "sim.h"
using namespace std;
using namespace forwardsec;
using namespace relicxx;
bytes testVector = {{0x3a, 0x5d, 0x7a, 0x42, 0x44, 0xd3, 0xd8, 0xaf, 0xf5, 0xf3, 0xf1, 0x87, 0x81, 0x82, 0xb2,
						  0x53, 0x57, 0x30, 0x59, 0x75, 0x8d, 0xe6, 0x18, 0x17, 0x14, 0xdf, 0xa5, 0xa4, 0x0b,0x43,0xAD,0xBC}};
std::vector<string>makeTags(unsigned int n,unsigned int startintag){
	std::vector<string> tags(n);
	for(unsigned int i=0;i<n;i++){
		tags[i] = "tag"+std::to_string(startintag+i);
	}
	return tags;
}

 
 void sim(){ 
	 unsigned int windowsize;
	 unsigned int depth;
	 unsigned int numtags;
	 unsigned int iterations;
	 string dbgstr;
	 std::cin >> windowsize >> depth >> numtags >> iterations >> dbgstr;
	// cerr << "window size: " << windowsize << " depth: " << depth << " numtags: " 
	// 	<< numtags << " iterations: " << iterations << endl;
	 CPUTimeAndAvg dec,punc,derive;
	 
	 GMPfse test(depth,numtags);
	 GMPfsePublicKey pk;
	 GMPfsePrivateKey sk;
	 test.keygen(pk,sk);
	 bytes msg = {{0x3a, 0x5d, 0x7a, 0x42, 0x44, 0xd3, 0xd8, 0xaf, 0xf5, 0xf3, 0xf1, 0x87, 0x81, 0x82, 0xb2,
						  0x53, 0x57, 0x30, 0x59, 0x75, 0x8d, 0xe6, 0x18, 0x17, 0x14, 0xdf, 0xa5, 0xa4, 0x0b,0x43,0xAD,0xBC}};
	int previnterval=0;
	int msg_interval=0;
	int tagctr = 42;
	unsigned int skSize = 0;
	unsigned int clockticks = 0;
	 int exceptionctr=0;
	 while(std::cin >> msg_interval){
		 	tagctr++;
		 	if(tagctr%10 == 0){
		 		cout << "." << endl;
		 	}
		 	if(msg_interval>previnterval ){
				//cerr << ".";
		 		clockticks++;
				stringstream ss;
				{
					cereal::PortableBinaryOutputArchive oarchive(ss);
					oarchive(sk);
				}
				skSize = std::max<int>(skSize,(unsigned int)ss.tellp());
		 		if(clockticks>windowsize){
		 			for(unsigned int toDelete = clockticks - windowsize;toDelete<clockticks;toDelete++){
			 			if(!sk.hasKey(toDelete)){
			 				continue;
			 			}
			 			if(sk.needsChildKeys(toDelete)){
			 	 			derive.start();
			 				test.prepareIntervalAfter(pk,sk,toDelete);
			 	 			derive.stop();
	 	 					derive.reg();
			 			}
			 			sk.erase(toDelete);
		 			}
		 		}
		 	}
		 	previnterval = msg_interval;
		 	auto tags = makeTags(numtags,tagctr);
	 	 	GMPfseCiphertext ct = test.encrypt(pk,msg,msg_interval,tags);
			if(!sk.hasKey(msg_interval)){
	 	 		GMPfsePrivateKey skcpy;
	 	 		try{
		 	 		for(unsigned int i =0;i<iterations;i++){
			 	 		skcpy = sk;
	 	 				derive.start();
	 	 				test.deriveKeyFor(pk,skcpy,msg_interval);
	 	 				derive.stop();
	 	 				derive.reg();
	 	 			}
	 	 			sk=skcpy;
 				} catch (std::invalid_argument e) {
			 		exceptionctr++;
					derive.reset();
					string what = e.what();
					ofstream errorfile;
					errorfile.open("error_der_"+dbgstr,ios::binary|ios::out);
					{
					cereal::PortableBinaryOutputArchive oarchive(errorfile);
					oarchive(what,msg_interval,pk,skcpy,ct);
					cerr << "Exception during keyder on msg_interval" << msg_interval << " " <<what << endl;
					}
					errorfile.close();
					continue;
				}
			}

			try{
	 	 		for(unsigned int i =0;i<iterations;i++){
		 		 	dec.start();
		 			bytes result = test.decrypt(pk,sk,ct);
		 	 		dec.stop();
			 	 	dec.reg();
		 	 	}
 			} catch (std::invalid_argument e) {
		 		exceptionctr++;
				dec.reset();
				string what = e.what();
				ofstream errorfile;

				errorfile.open("error_der_"+dbgstr,ios::binary|ios::out);
				{
				cereal::PortableBinaryOutputArchive oarchive(errorfile);
				oarchive(what,msg_interval,pk,sk,ct);
				cerr << "Exception during decrypt for interval " << msg_interval << " " <<  what << endl;
 	    		}
 	    		errorfile.close();
				continue;

			 }
	 	 	for(auto t: tags){
	 	 		GMPfsePrivateKey skcpy;
		 	 	for(unsigned int i =0;i<iterations;i++){
		 	 		skcpy=sk;
		 	 		if(skcpy.needsChildKeys(msg_interval)){
		 	 			derive.start();
		 	 			test.prepareIntervalAfter(pk,skcpy,msg_interval);
		 	 			derive.stop();
		 	 			derive.reg();
		 	 		}
	 	 			punc.start();
	 	 			test.puncture(pk,skcpy,msg_interval,t);
	 	 			punc.stop();
	 	 			punc.reg();
	 	 		}
	 	 		sk=skcpy;
	 	 	}


	}
	cerr << endl;

	cout << "DeriveTime\n\t" << derive << endl;
	cout << "DecTime\n\t" << dec << endl;
	cout << "PunTime\n\t" << punc << endl;
	cout << "MaxSize\t" << skSize << endl;

 }
 int main(){
 	relicResourceHandle h;
 	sim();
 }
