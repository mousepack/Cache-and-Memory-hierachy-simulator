#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <vector>

using namespace std;

string::size_type sz;
long opCount = 1;
long counter = 0;

void l1Read (string);
void l2Read (string);
void l1Write (string, int);
void l2Write (string, int);

// Conversion of hex adddress to tag and index
struct addressf{
    string addBlock;
    string tag;
    int index;
};

addressf makeAddress(string addHex, int blockSizeFinal, int numSets){
    long decAdd = stoi(addHex, &sz, 16);
    int blockBits = ceil(log2(blockSizeFinal));
    int setBits = ceil(log2(numSets));
    long addBlock  = decAdd % long(pow(2, blockBits));
    addBlock = decAdd - addBlock;
    long tagAdd = decAdd / long(pow(2, setBits+blockBits));
    int index = (decAdd % long(pow(2, setBits+blockBits))) / long(pow(2, blockBits));
    addressf newAdd;
    std::stringstream sstream;
    sstream << std::hex << tagAdd;
    newAdd.tag = sstream.str();
    std::stringstream sstream2;
    sstream2 << std::hex << addBlock;
    newAdd.addBlock =  sstream2.str();
    newAdd.index =  index;
    return(newAdd);
}


//Initial Parameters
int blockSizeFinal;
int l1SizeFinal;
int l1AssocFinal;
int l2SizeFinal;
int l2AssocFinal;
int rPolicyFinal;
int iPolicyFinal;
string traceFile;

//Performance parameters
int readsL1 = 0;
int missesL1Read = 0;
int writesL1 = 0;
int missesL1Write = 0;
int missRateL1 = 0;
int writeBacksL1 = 0;
int readsL2 = 0;
int missesL2Read = 0;
int writesL2 = 0;
int missesL2Write = 0;
int missRateL2 = 0;
int writeBacksL2 = 0;
int memoryTraffic = 0;
int directWriteBacksL1 = 0;

//Block parameters
struct block
{
    string addBlock;
    string addHex;
    string tag;
    int index;
    bool valid;
    long serial;
    long dirtyBit;
};

//Serial Number counters
long l1SerialFinal = 1;
long l2SerialFinal = 1;


long setsL1;
long setsL2;
block** l1;
block**l2;

struct fileContent{
    string ops;
    string addHex;
};

// Vector of trace file lines. 
vector<fileContent*> contentTrace;

void l1initialise()
{
    //Create L1 Cache
    setsL1 = l1SizeFinal / (blockSizeFinal * l1AssocFinal);
    l1 = new block*[setsL1];
    for (int i=0; i<setsL1; i++){
        l1[i] = new block[l1AssocFinal];
    }
    //Default values
    for (int i=0; i<setsL1; i++)
    {
        for (int j=0; j<l1AssocFinal; j++)
        {
            l1[i][j].addBlock = "";
            l1[i][j].addHex = "";
            l1[i][j].index = i;
            l1[i][j].tag = "";
            l1[i][j].valid = 0;
            l1[i][j].serial = 0;
            l1[i][j].dirtyBit = 0;
        }
    }
}
void l2initialise()
{
    if (l2SizeFinal>0)
    {
        setsL2 = l2SizeFinal / (blockSizeFinal*l2AssocFinal);
        l2 = new block*[setsL2];
        for (int i=0; i<setsL2; i++){
            l2[i] = new block[l2AssocFinal];
        }
        //Set default values
        for (int i=0; i<setsL2; i++)
        {
            for (int j=0; j<l2AssocFinal; j++)
            {
                l2[i][j].addBlock = "";
                l2[i][j].addHex = "";
                l2[i][j].index = i;
                l2[i][j].tag = "";
                l2[i][j].valid = 0;
                l2[i][j].serial = 0;
                l2[i][j].dirtyBit = 0;
            }
        }
    }
}

//Creating object out of each line in trace.
fileContent* getfileContent(string ops, string addr)
{
    fileContent *temp = new fileContent();
    temp->addHex = addr;
    temp->ops = ops;
    return temp;
}

//Read Whole trace file and make vector out of it.
void readTraceFile(string traceFileName)
{ 
    ifstream infile(traceFileName);
    string op;
    string addHex;
    while(infile >> op >> addHex )
    {
        contentTrace.push_back(getfileContent(op,addHex));
    }
}

void optimal(vector<fileContent*> &contentTrace, int __start, vector<string> &__set, int blockSizeFinal, int numSets)
{   
   for(int i =__start ; i<contentTrace.size();i++)
   {
       if(__set.size() == 1)
            return;
        for(int j = 0;j<__set.size();j++)
        {
            addressf newAdd = makeAddress(contentTrace[i]->addHex, blockSizeFinal, numSets);
            if(__set[j] == newAdd.addBlock) //
            {
                __set.erase(__set.begin()+j);
                break;
            }
        }
   }
   return;
}


//l1 Invalidate
bool l1Invalidate(string addHex){
    addressf newAdd = makeAddress(addHex, blockSizeFinal, setsL1);
    for (int j=0; j<l1AssocFinal; j++){
        if (l1[newAdd.index][j].addBlock==newAdd.addBlock){
            if (l1[newAdd.index][j].valid == true){
                l1[newAdd.index][j].valid = false;
                if (l1[newAdd.index][j].dirtyBit == 1){
                    //cout<<"L1 invalidated: "<<newAdd.addBlock<<" (tag "<<newAdd.tag<<", index "<<newAdd.index<<", dirty)"<<endl;
                    //cout<<"L1 writeback to main memory directly"<<endl;
                    //writeBacksL1 += 1;
                    directWriteBacksL1 += 1;
                    return(true);
                }
                if (l1[newAdd.index][j].dirtyBit == 0){
                    //cout<<"L1 invalidated: "<<newAdd.addBlock<<" (tag "<<newAdd.tag<<", index "<<newAdd.index<<", clean)"<<endl;
                    return(false);
                }
            }
        }
    }
    return(false);
}

void displayL1()
{
    cout<<"===== L1 contents ====="<<endl;
    for (int i=0; i<setsL1; i++){
        cout<<"Set"<<"\t"<<i<<":"<<"\t";
        for (int j=0; j<l1AssocFinal; j++){
            cout<<l1[i][j].tag;
            if (l1[i][j].dirtyBit == 1){
                cout<<" "<<"D";
            }
            cout<<"    ";
        }
        cout<<endl;
    }
}

void displayL2()
{
    if (l2SizeFinal>0){
        cout<<"===== L2 contents ====="<<endl;
        for (int i=0; i<setsL2; i++){
            cout<<"Set"<<"\t"<<i<<":"<<"\t";
            for (int j=0; j<l2AssocFinal; j++){
                cout<<l2[i][j].tag;
                if (l2[i][j].dirtyBit == 1){
                    cout<<" "<<"D   ";
                }
                else{
                    cout<<"    ";
                }
            }
            cout<<endl;
        }
    }
}

//L1 read function
void l1Read(string addHex){

    //Get the tag and index
    addressf newAdd = makeAddress(addHex, blockSizeFinal, setsL1);
    
    //Attempt L1 Read
    //cout<<"L1 read : "<<newAdd.addBlock<<" "<<"(tag "<<newAdd.tag<<", "<<"index "<<newAdd.index<<")"<<endl;
    readsL1 += 1;
    for (int i=0; i<l1AssocFinal; i++){
        if (l1[newAdd.index][i].tag == newAdd.tag && l1[newAdd.index][i].valid==true){
            //cout<<"L1 hit"<<endl;
            //LRU
            if (rPolicyFinal==0){
                //cout<<"L1 update LRU"<<endl;
                l1[newAdd.index][i].serial = l1SerialFinal++;
            }
            if (rPolicyFinal==1){
                //cout<<"L1 update FIFO"<<endl;
            }
            if (rPolicyFinal==2){
                l1[newAdd.index][i].serial = l1SerialFinal++;
                //cout<<"L1 update optimal"<<endl;
            }
            return;
        }
    }
    //Not present in L1
    missesL1Read += 1;
    //readsL2 += 1;
    //cout<<"L1 miss"<<endl;

    //Fetch to L1 cache
    //writesL1 += 1;
    l1Write(addHex, 0);
}

// L1 write
void l1Write(string addHex, int write){
    
    //Get the tag and index
    addressf newAdd = makeAddress(addHex, blockSizeFinal, setsL1);

    //Print exlicit write status is so
    if (write==1){
        //cout<<"L1 write : "<<newAdd.addBlock<<" "<<"(tag "<<newAdd.tag<<", "<<"index "<<newAdd.index<<")"<<endl;
    }

    //If explicit write
    if (write==1){
        writesL1 += 1;
        //Check if the address is available, then set the dirty bit to true and return
        for (int i=0; i<l1AssocFinal; i++){
            if (l1[newAdd.index][i].tag == newAdd.tag && l1[newAdd.index][i].valid==true){
                //cout<<"L1 hit"<<endl;
                //LRU
                if (rPolicyFinal==0){
                    //cout<<"L1 update LRU"<<endl;
                    l1[newAdd.index][i].serial = l1SerialFinal++;
                }
                if (rPolicyFinal==1){
                    //cout<<"L1 update FIFO"<<endl;
                }
                if (rPolicyFinal==2){
                    l1[newAdd.index][i].serial = l1SerialFinal++;
                    //cout<<"L1 update optimal"<<endl;
                }
                //Set dirty bit
                l1[newAdd.index][i].dirtyBit = 1;
                //cout<<"L1 set dirty"<<endl;
                return;
            }
        }
        missesL1Write += 1;
        //readsL2 += 1;
        //cout<<"L1 miss"<<endl;
    }
    //Else:
    //Determine the victim (empty slot or the minimum serial)
    long victimSerial = 2147483647;
    if (rPolicyFinal==0 || rPolicyFinal==1){
        for (int i=0; i<l1AssocFinal; i++){
            if (l1[newAdd.index][i].valid==false){
                victimSerial = l1[newAdd.index][i].serial;
                break;
            }
            else {
                victimSerial = min(victimSerial, l1[newAdd.index][i].serial);
            }
        }
    }

    if (rPolicyFinal==2){
        //Check if an invalid slot is available
        for (int j=0; j<l1AssocFinal; j++){
            if (l1[newAdd.index][j].valid==false){
                victimSerial = l1[newAdd.index][j].serial;
                break;
            }
        }
        //Determine optimal replacement in case no invalid slots are avialable
        if (victimSerial==2147483647){
            vector<string> set;
            for(int j=0; j<l1AssocFinal; j++){
                set.push_back(l1[newAdd.index][j].addBlock);
            }

            optimal(contentTrace, counter + 1, set, blockSizeFinal, setsL1);
            if (set.size()>1){
                //cout<<"Arbitrary"<<endl;
            }
            string victimaddBlock = set[0];
            for (int j=0; j<l1AssocFinal; j++){
                if (l1[newAdd.index][j].addBlock==victimaddBlock){
                    victimSerial = l1[newAdd.index][j].serial;
                    break;
                }
            }
        }
    }
    
    //Insert in the slot
    for (int i=0; i<l1AssocFinal; i++){
        if (l1[newAdd.index][i].serial==victimSerial){
            //If empty slot
            if (l1[newAdd.index][i].valid==false){
                //cout<<"L1 victim: none"<<endl;
            }
            else{
                //if victim dirty bit is set, send write to L2
                if (l1[newAdd.index][i].dirtyBit==1){
                    //cout<<"L1 victim: "<<l1[newAdd.index][i].addBlock<<" (tag "<<l1[newAdd.index][i].tag<<", index "<<l1[newAdd.index][i].index<<", dirty"<<")"<<endl;
                    l1[newAdd.index][i].valid = false;
                    writeBacksL1 += 1;
                    l2Write(l1[newAdd.index][i].addHex, 1);   
                }
                else{
                    //cout<<"L1 victim: "<<l1[newAdd.index][i].addBlock<<" (tag "<<l1[newAdd.index][i].tag<<", index "<<l1[newAdd.index][i].index<<", clean"<<")"<<endl;
                    l1[newAdd.index][i].valid = false;
                }
            }

            //Issue a read request to L2
             l2Read(addHex);
            
            l1[newAdd.index][i].valid = true;
            l1[newAdd.index][i].addHex = addHex;
            l1[newAdd.index][i].addBlock = newAdd.addBlock;
            l1[newAdd.index][i].tag = newAdd.tag;
            l1[newAdd.index][i].index = newAdd.index;
            l1[newAdd.index][i].serial = l1SerialFinal++;
            if (rPolicyFinal==0){
                //cout<<"L1 update LRU"<<endl;
            }
            if (rPolicyFinal==1){
                //cout<<"L1 update FIFO"<<endl;
            }
            if (rPolicyFinal==2){
                //cout<<"L1 update optimal"<<endl;
            }
            // If write, then set dirty bit, else just read, so don't set dirty bit
            if (write==1){
                //cout<<"L1 set dirty"<<endl;
                l1[newAdd.index][i].dirtyBit = 1;
            }
            else {
                l1[newAdd.index][i].dirtyBit = 0;
            }
            break;
        }
    }
}

//L2 read function
void l2Read(string addHex)
{
    if (l2SizeFinal==0)
    {
        return;
    }
    
    //Get the tag and index
    addressf newAdd = makeAddress(addHex, blockSizeFinal, setsL2);
    
    //Attempt L2 Read
    //cout<<"L2 read : "<<newAdd.addBlock<<" "<<"(tag "<<newAdd.tag<<", "<<"index "<<newAdd.index<<")"<<endl; 
    readsL2 += 1;
    for (int i=0; i<l2AssocFinal; i++)
    {
        if (l2[newAdd.index][i].tag == newAdd.tag && l2[newAdd.index][i].valid==true)
        {
            //cout<<"L2 hit"<<endl;
            //LRU
            if (rPolicyFinal==0)
            {
                //cout<<"L2 update LRU"<<endl;
                l2[newAdd.index][i].serial = l2SerialFinal++;
            }
            if (rPolicyFinal==1)
            {
                //cout<<"L1 update FIFO"<<endl;
            }
            if (rPolicyFinal==2)
            {
                //cout<<"L2 update optimal"<<endl;
                l2[newAdd.index][i].serial = l2SerialFinal++;
            }
            return;
        }
    }
    //Not present in L2
    missesL2Read += 1;
    //cout<<"L2 miss"<<endl;

    //Fetch to L2 caches
    l2Write(addHex, 0);
}

//L2 write
void l2Write(string addHex, int write)
{
    if (l2SizeFinal==0)
    {
        return;
    }
    
    //Get the tag and index
    addressf newAdd = makeAddress(addHex, blockSizeFinal, setsL2);

    if (write==1)
    {
        //cout<<"L2 write : "<<newAdd.addBlock<<" (tag "<<newAdd.tag<<", index "<<newAdd.index<<")"<<endl;
        writesL2 += 1;
    }

    //If explicit write
    if (write==1)
    {
        //Check if the address is available, then set the dirty bit to true and return
        for (int i=0; i<l2AssocFinal; i++)
        {
            if (l2[newAdd.index][i].tag == newAdd.tag && l2[newAdd.index][i].valid==true)
            {
                //cout<<"L2 hit"<<endl;
                //LRU
                if (rPolicyFinal==0)
                {
                    //cout<<"L2 update LRU"<<endl;
                    l2[newAdd.index][i].serial = l2SerialFinal++;
                }
                if (rPolicyFinal==1)
                {
                //cout<<"L2 update FIFO"<<endl;
                }
                if (rPolicyFinal==2)
                {
                //cout<<"L2 update optimal"<<endl;
                l2[newAdd.index][i].serial = l2SerialFinal++;
                }
                //cout<<"L2 set dirty"<<endl;
                l2[newAdd.index][i].dirtyBit = 1;
                return;
            }
        }
        missesL2Write += 1;
        //cout<<"L2 miss"<<endl;
    }
    //Else:
    //Determine the victim (empty slot or the minimum serial)
    long victimSerial = 2147483647;
    
    if (rPolicyFinal==0 || rPolicyFinal==1)
    {
        for (int i=0; i<l2AssocFinal; i++)
        {
            if (l2[newAdd.index][i].valid==false)
            {
                victimSerial = l2[newAdd.index][i].serial;
                break;
            }
            else 
            {
                victimSerial = min(long(victimSerial), l2[newAdd.index][i].serial);
            }
        }
    }

    if (rPolicyFinal==2)
    {
        //Check if an invalid slot is available
        for (int j=0; j<l2AssocFinal; j++)
        {
            if (l2[newAdd.index][j].valid==false)
            {
                victimSerial = l2[newAdd.index][j].serial;
                break;
            }
        }
        //Determine optimal replacement in case no invalid slots are avialable
        if (victimSerial==2147483647){
            vector<string> set;
            for(int j=0; j<l2AssocFinal; j++)
            {
                set.push_back(l2[newAdd.index][j].addBlock);
            }
            optimal(contentTrace, counter + 1, set, blockSizeFinal, setsL2);
            string victimaddBlock= set[0];
            for (int j=0; j<l1AssocFinal; j++)
            {
                if (l2[newAdd.index][j].addBlock==victimaddBlock)
                {
                    victimSerial = l2[newAdd.index][j].serial;
                    break;
                }
            }
        }
    }



    //Insert in the slot
    for (int i=0; i<l2AssocFinal; i++){
        if (l2[newAdd.index][i].serial==victimSerial)
        {
            //Check for valid and print status accordingly
            if (l2[newAdd.index][i].valid==true){
                if (l2[newAdd.index][i].dirtyBit==true)
                {
                    //cout<<"L2 victim: "<<l2[newAdd.index][i].addBlock<<" (tag "<<l2[newAdd.index][i].tag<<", index "<<l2[newAdd.index][i].index<<", dirty"<<")"<<endl;
                    
                    if (iPolicyFinal==1)
                    {
                        bool r = l1Invalidate(l2[newAdd.index][i].addHex);
                        if (r==false)
                        {
                            writeBacksL2 += 1;
                        }
                    }

                    else 
                    {
                        writeBacksL2 += 1;
                    }
                }
                else
                {
                    //cout<<"L2 victim: "<<l2[newAdd.index][i].addBlock<<" (tag "<<l2[newAdd.index][i].tag<<", index "<<l2[newAdd.index][i].index<<", clean"<<")"<<endl;
                    if (iPolicyFinal==1)
                    {
                        l1Invalidate(l2[newAdd.index][i].addHex);
                    }
                }
            }
            else
            {
                //cout<<"L2 victim: none"<<endl;
            }
            l2[newAdd.index][i].addBlock = newAdd.addBlock;
            l2[newAdd.index][i].index = newAdd.index;
            l2[newAdd.index][i].addHex = addHex;
            l2[newAdd.index][i].tag = newAdd.tag;
            l2[newAdd.index][i].serial = l2SerialFinal++;
            if (rPolicyFinal==0)
            {
                //cout<<"L2 update LRU"<<endl;
            }
            if (rPolicyFinal==1)
            {
                //cout<<"L2 update FIFO"<<endl;
            }
            if (rPolicyFinal==2)
            {
                //cout<<"L2 update optimal"<<endl;
            }

            l2[newAdd.index][i].valid = 1;
            // If write, then set dirty bit, else just read, so don't set dirty bit
            if (write==1){
                //cout<<"L2 set dirty"<<endl;
                l2[newAdd.index][i].dirtyBit = 1;
            }
            else {
                l2[newAdd.index][i].dirtyBit = 0;
            }
            break;
        }
    }
}


int main(int argc, char* argv[])
{
    string blockSizeFinalStr = argv[1];
    string l1SizeFinalStr = argv[2];
    string l1AssocFinalStr = argv[3];
    string l2SizeFinalStr = argv[4];
    string l2AssocFinalStr = argv[5];
    string rPolicyFinalStr = argv[6];
    string iPolicyFinalStr = argv[7];
    traceFile = argv[8];

    blockSizeFinal = stoi(blockSizeFinalStr, &sz);
    l1SizeFinal = stoi(l1SizeFinalStr, &sz);
    l1AssocFinal = stoi(l1AssocFinalStr, &sz);
    l2SizeFinal = stoi(l2SizeFinalStr, &sz);
    l2AssocFinal = stoi(l2AssocFinalStr, &sz);
    rPolicyFinal = stoi(rPolicyFinalStr, &sz);
    iPolicyFinal = stoi(iPolicyFinalStr, &sz);

    //Initial configuration
    cout<<"===== Simulator configuration ====="<<"\n";
    cout<<"BLOCKSIZE:        	  "<<blockSizeFinal<<"\n";
    cout<<"L1_SIZE:               "<<l1SizeFinal<<"\n";
    cout<<"L1_ASSOC:              "<<l1AssocFinal<<"\n";
    cout<<"L2_SIZE:               "<<l2SizeFinal<<"\n";
    cout<<"L2_ASSOC:              "<<l2AssocFinal<<"\n";
    if (rPolicyFinal==0){
        cout<<"REPLACEMENT POLICY:    "<<"LRU"<<"\n";
    }
    else if (rPolicyFinal==1){
        cout<<"REPLACEMENT POLICY:    "<<"FIFO"<<"\n";
    }
    else if (rPolicyFinal==2){
        cout<<"REPLACEMENT POLICY:    "<<"optimal"<<"\n";
    }
    
    if (iPolicyFinal==0){
        cout<<"INCLUSION PROPERTY:    "<<"non-inclusive"<<"\n";
    }
    else if (iPolicyFinal==1){
        cout<<"INCLUSION PROPERTY:    "<<"inclusive"<<"\n";
    }
    
    cout<<"trace_file:            "<<traceFile<<"\n";

    //Create L1 Cache
    l1initialise();
    //Create L2 Cache
    l2initialise();

    //Open and read trace file
    readTraceFile(traceFile);
    for(counter = 0; counter<contentTrace.size(); counter++){
        string op = contentTrace[counter]->ops;
        string addHex = contentTrace[counter]->addHex;
        if (op=="r"){
            //cout<<"----------------------------------------"<<endl;
            //cout<<"#"<<" "<<opCount++<<" "<<":"<<" "<<"read"<<" "<<addHex<<endl;
            //readsL1 += 1;
            l1Read(addHex);
           
        }
        if (op=="w"){
            //cout<<"----------------------------------------"<<endl;
            //cout<<"#"<<" "<<opCount++<<" "<<":"<<" "<<"write"<<" "<<addHex<<endl;
            //writesL1 += 1;
            l1Write(addHex, 1);
            
        }
    }

    //Print L1 content
    displayL1();

    //Print L2 content
    displayL2();

    //Calculations
    float missRateL1 = float(missesL1Read + missesL1Write) / float(readsL1 + writesL1);
    float missRateL2 = 0;
    if (l2SizeFinal>0)
    {
        missRateL2 = float(missesL2Read)/float(readsL2);
    }
    
    if (l2SizeFinal==0)
    {
        memoryTraffic = missesL1Read + missesL1Write + writeBacksL1;
    }

    if (l2SizeFinal>0 && iPolicyFinal==0)
    {
        memoryTraffic = missesL2Read + missesL2Write + writeBacksL2;
    }

    if (l2SizeFinal>0 && iPolicyFinal==1)
    {
        memoryTraffic = missesL2Read + missesL2Write + writeBacksL2 + directWriteBacksL1;
    }


    //Output
    cout<<"===== Simulation results (raw) ====="<<endl;
    cout<<"a. number of L1 reads:        "<<readsL1<<endl;
    cout<<"b. number of L1 read misses:  "<<missesL1Read<<endl;
    cout<<"c. number of L1 writes:       "<<writesL1<<endl;
    cout<<"d. number of L1 write misses: "<<missesL1Write<<endl;
    cout<<"e. L1 miss rate:              "<<fixed<<setprecision(6)<<missRateL1<<endl;
    cout<<"f. number of L1 writebacks:   "<<writeBacksL1<<endl;
    cout<<"g. number of L2 reads:        "<<readsL2<<endl;
    cout<<"h. number of L2 read misses:  "<<missesL2Read<<endl;
    cout<<"i. number of L2 writes:       "<<writesL2<<endl;
    cout<<"j. number of L2 write misses: "<<missesL2Write<<endl;
    if (missRateL2==0){
        cout<<"k. L2 miss rate:              "<<int(missRateL2)<<endl;
    }
    else{
        cout<<"k. L2 miss rate:              "<<fixed<<setprecision(6)<<missRateL2<<endl;
    }
    cout<<"l. number of L2 writebacks:   "<<writeBacksL2<<endl;
    cout<<"m. total memory traffic:      "<<memoryTraffic<<endl;
}