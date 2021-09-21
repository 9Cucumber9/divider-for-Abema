#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>
#include <cmath>
#include <filesystem>

int loadTsPacket( std::ifstream*, unsigned int* ,size_t);
int read_PAT_Info( unsigned int* );
int search_Copyleft( unsigned int* );
int generateOutputPath( char*, int, int, std::string*);

int loadTsPacket( std::ifstream* ifs_ptr , unsigned int *tsPacketBytes , size_t seek){
  char temp[188];   //バイナリをここに一時的に代入

  if(!(*ifs_ptr)){                             //ファイルが無ければ終了
    std::ofstream errorLog( "error.txt" , std::ios::app);
    std::cout << "read Error\n";
    std::cout << (seek/188) << "\n";
    errorLog << "read Error" << "\n" << "could not read the file.\n";
    exit (1);
  }
  ifs_ptr->seekg( seek , std::ios::beg );
  ifs_ptr->read( temp , 188);

  for (int i=0;i<188;i++){
    if(temp[i]>=0){
      tsPacketBytes[i]=0+temp[i];
    }else{
      tsPacketBytes[i]=temp[i]+0x100;       //128以上は負の数になっているので足して元の数値に戻す
    }
  }

  if(tsPacketBytes[0]!=0x47){               //sync_byteが見つからなければ終了
    std::ofstream errorLog( "error.txt" , std::ios::app);
    std::cout << "'0x47' was not found\n";
    errorLog << "'0x47' was not found\nおそらくtsファイルではない、もしくはデータに欠損があります\nMaybe it's not a TS file , or some data are missing.\n";
    errorLog << "Address(approximately):" << std::hex << seek << "\n";
    exit (1);
  }
  return 0;
}

int read_PAT_Info( unsigned int *tsPacketBytes ){
  unsigned int PID;
  unsigned int sectionLength;
  unsigned int transport_streamID;
  unsigned int programNumber;
  unsigned int PMT_PID;
  PID =                (tsPacketBytes[1]*0x100+tsPacketBytes[2])&0x1FFF;     //末尾の13bitを取る
  sectionLength =      (tsPacketBytes[6]*0x100+tsPacketBytes[7])&0xFFF;      //末尾の12bitを取る
  transport_streamID = (tsPacketBytes[8]*0x100+tsPacketBytes[9]);
  programNumber =      (tsPacketBytes[13]*0x100+tsPacketBytes[14]);
  PMT_PID =            (tsPacketBytes[15]*0x100+tsPacketBytes[16])&0x1FFF;   //末尾の13bitを取る
  std::cout << "PID:"                << std::dec << PID << "\n";
  std::cout << "sectionLength:"      << std::dec << sectionLength << "\n";
  std::cout << "transport_streamID:" << std::dec << transport_streamID << "\n";
  std::cout << "programNumber:"      << std::dec << programNumber << "\n";
  std::cout << "PMT_PID:"            << std::dec << PMT_PID << "\n";
  return PMT_PID;
}

int search_Copyleft(unsigned int* tsPacketBytes){     //(C,o,p,y,l,e,f,t)==(0x43,0x6f,0x70,0x79,0x6c,0x65,0x66,0x74)
  for(int i=0 ; i<181 ; i++){
    if((tsPacketBytes[i]==0x43)&&(tsPacketBytes[i+1]==0x6f)&&(tsPacketBytes[i+2]==0x70)&&(tsPacketBytes[i+3]==0x79)){
      if((tsPacketBytes[i+4]==0x6c)&&(tsPacketBytes[i+5]==0x65)&&(tsPacketBytes[i+6]==0x66)&&(tsPacketBytes[i+7]==0x74)){
        return 1;
      }
    }
  }
  return 0;
}

int generateOutputPath( char* input, int chapter, int section, std::string* output){
  std::string inputStr = input;
  std::stringstream insertStr;
  insertStr << "_" << std::setw(2) << std::setfill('0') << chapter << "-" << std::setw(2) << std::setfill('0') << section;
  if( inputStr.rfind(".") == std::string::npos ){
    std::ofstream errorLog( "error.txt" ,std::ios::app);
    std::cout << "File extension was not found\n";
    errorLog << "File extension was not found\n";
    exit (1);
  }
  *output = inputStr.insert(inputStr.rfind(".") , insertStr.str() );
  return 0;
}



int main(int argc , char* argv[]){
  unsigned int* tsPacketBytes = new unsigned int[188];
  size_t fileSize;
  size_t packetNumber=0;
  char writeData[188];
  int previousPMT_PID=-1;
  int currentPMT_PID=0;
  int chapter=1;
  int section=1;
  std::string outputPath;
  std::ifstream  ifs;
  std::ifstream* ifs_ptr=&ifs;
  std::ofstream  result;
  std::ofstream  output;
  fileSize = std::filesystem::file_size( argv[1] );
  generateOutputPath(argv[1], chapter, section, &outputPath);
  ifs.open( argv[1] , std::ios::binary );
  output.open(outputPath , std::ios::binary | std::ios::app);
  result.open( "result.txt" , std::ios::app );
  result << "###\n###" << argv[1] << "\n###\n";

  while( (fileSize/188) > packetNumber ){          //ファイルの末端に到達で終了
    loadTsPacket( ifs_ptr, tsPacketBytes, packetNumber*188 );

    if( ((tsPacketBytes[1]*0x100+tsPacketBytes[2])&0x1FFF)==0 ){  //PID=0、つまりPATパケットのときtrue
      if ( (packetNumber*188/1024) < 100000){
        std::cout << "====PAT====\n" << "KiloByte:" << std::setw(6) << std::setfill('0') << std::dec 
                  << (packetNumber*188)/1024 << "/" << fileSize/1024 << "\n";
      }else{
        std::cout << "====PAT====\n" << "MegaByte:" << std::fixed << std::setprecision(1) << std::setw(6) << std::setfill('0') << std::dec 
                  << (packetNumber*188)/std::pow(1024 , 2) << "/" << fileSize/std::pow(1024,2) << "\n";
      }
      currentPMT_PID=read_PAT_Info(tsPacketBytes);
      std::cout << "===========\n";

      if( (previousPMT_PID != currentPMT_PID) && ( previousPMT_PID != -1 ) ){   //1つ前と今のPMT_PIDが異なり、かつpreviousPMT_PIDが初期値ではないときtrue
        result << std::dec << "packet:" << std::setw(8) << std::setfill('0') << packetNumber << "(Address:" << std::setw(9) << std::setfill('0') << std::hex << packetNumber*188 << ") : ";
        result << std::dec << "PMT_PID change(" << previousPMT_PID << "->" << currentPMT_PID << ")\n";
        section = section + 1;
        output.close();
        generateOutputPath(argv[1] ,chapter ,section ,&outputPath);
      }
      previousPMT_PID=currentPMT_PID;
    }

    if( (fileSize/188) > (packetNumber+3) ){
      loadTsPacket( ifs_ptr ,tsPacketBytes ,(packetNumber+2)*188 );    //二つ先のパケットに「Copyleft」がないか検索
      if(search_Copyleft(tsPacketBytes)==1){
        result << std::dec << "packet:" << std::setw(8) << std::setfill('0') << (packetNumber-2) << "(Address:" << std::setw(9) << std::setfill('0') << std::hex << (packetNumber-2)*188 << ") : Maybe new program start\n";
        chapter = chapter + 1;
        section = 1;
        output.close();
        generateOutputPath(argv[1] ,chapter ,section ,&outputPath);
      }
    }

    loadTsPacket( ifs_ptr, tsPacketBytes ,packetNumber*188 );
    if ( !output.is_open() ){
      output.open(outputPath , std::ios::binary | std::ios::app);
    }
    for(int i=0 ; i<=187 ; i++){      //charに入れてから書き込み
      writeData[i]=tsPacketBytes[i];
    }
    output.write( writeData ,188 );
    packetNumber=packetNumber+1;
  }

  output.close();
  ifs.close();
  result.close();
  delete[] tsPacketBytes;
  return 0;
}
