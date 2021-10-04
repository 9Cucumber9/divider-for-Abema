#define _FILE_OFFSET_BITS 64

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>
#include <cmath>
#include <time.h>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <sys/stat.h>

int loadTsPacket( FILE*, unsigned char*, unsigned long long, unsigned long long);
int read_PAT_Info( unsigned char*, int, int, int&);
int checkPCR( unsigned char*, int, int, unsigned long long*);
int search_Copyleft( unsigned char*, int, int);
int generateOutputPath( char*, int, int, std::string*);

int loadTsPacket( FILE* inputFile_ptr, unsigned char* tsPacketBytes, unsigned long long fileSize, unsigned long long seek){
  fread(tsPacketBytes, 1, 1880, inputFile_ptr);
  for(int i=0; i<10; i++){
    if( (seek + i*188) < fileSize){
      if(tsPacketBytes[0+i*188]!=0x47){               //sync_byteが見つからなければ終了
        std::ofstream errorLog( "error.txt", std::ios::app);
        std::cout << "'0x47' was not found\n";
        errorLog << "'0x47' was not found\nおそらくtsファイルではない、もしくはデータに欠損があります\nMaybe it's not a TS file , or some data are missing.\n";
        errorLog << "Address(approximately):" << std::hex << seek+(i*188) << "\n";
        errorLog.close();
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        exit (1);
      }
    }else{
      for(int j=0; j<188; j++){
        tsPacketBytes[j+i*188]=0;
      }
    }
  }
  return 0;
}

int read_PAT_Info( unsigned char* tsPacketBytes, int packetNumber, int i, int& previousPMT_PID ){
  unsigned int PID;
  unsigned int sectionLength;
  unsigned int transport_streamID;
  unsigned int programNumber;
  unsigned int currentPMT_PID;
  PID =                (tsPacketBytes[1+(i*188)]*0x100+tsPacketBytes[2+(i*188)])&0x1FFF;     //末尾の13bitを取る
  sectionLength =      (tsPacketBytes[6+(i*188)]*0x100+tsPacketBytes[7+(i*188)])&0xFFF;      //末尾の12bitを取る
  transport_streamID = (tsPacketBytes[8+(i*188)]*0x100+tsPacketBytes[9+(i*188)]);
  programNumber =      (tsPacketBytes[13+(i*188)]*0x100+tsPacketBytes[14+(i*188)]);
  currentPMT_PID =     (tsPacketBytes[15+(i*188)]*0x100+tsPacketBytes[16+(i*188)])&0x1FFF;   //末尾の13bitを取る
  if( (previousPMT_PID != currentPMT_PID) && ( previousPMT_PID != -1 ) ){                    //PMT_PIDが変化したとき
    previousPMT_PID = currentPMT_PID;
    return 1;
  }
  previousPMT_PID = currentPMT_PID;
  return 0;
}

int checkPCR( unsigned char* tsPacketBytes, int packetNumber, int i, unsigned long long* PCR){
  unsigned long long materialOfPCR=0;
  if( tsPacketBytes[3+(i*188)]&0x20 ){  //adaptation field controlが11,10のとき
    if( (tsPacketBytes[4+(i*188)] > 0)&&(tsPacketBytes[5+(i*188)]&0x10) ){      //adaptation field lengthが0以上かつ、PCR flagが1のとき
      for(int k=0; k<6; k++){
      materialOfPCR = materialOfPCR << 8;
      materialOfPCR = materialOfPCR + tsPacketBytes[6+(i*188)+k];
      }
      if( (((materialOfPCR&0xFFFFFFFF8000) >> 15)*300) < *PCR){                 //PCRが減少したとき
        *PCR = ((materialOfPCR&0xFFFFFFFF8000) >> 15)*300;
        return 1;
      }else{
        *PCR = ((materialOfPCR&0xFFFFFFFF8000) >> 15)*300;
      }
    }
  }
  return 0;
}

int search_Copyleft(unsigned char* tsPacketBytes, int packetNumber, int i){     //(C,o,p,y,l,e,f,t)==(0x43,0x6f,0x70,0x79,0x6c,0x65,0x66,0x74)
  for(int j=0; j<(188-7); j++){
    if((tsPacketBytes[j+(i*188)]==0x43)&&(tsPacketBytes[j+1+(i*188)]==0x6f)&&(tsPacketBytes[j+2+(i*188)]==0x70)&&(tsPacketBytes[j+3+(i*188)]==0x79)){
      if((tsPacketBytes[j+4+(i*188)]==0x6c)&&(tsPacketBytes[j+5+(i*188)]==0x65)&&(tsPacketBytes[j+6+(i*188)]==0x66)&&(tsPacketBytes[j+7+(i*188)]==0x74)){
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
    errorLog.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    exit (1);
  }
  *output = inputStr.insert( inputStr.rfind("."), insertStr.str() );
  return 0;
}



int main( int argc, char* argv[]){
  unsigned char* tsPacketBytes = new unsigned char[1880];
  unsigned long long fileSize;
  unsigned long long packetNumber=0;
  double progress;
  clock_t startTime;
  int PAT=0;
  int PMT_PID=-1;
  int previousPMT_PID=-1;
  unsigned long long PCR=0;
  unsigned long long previousPCR=0;
  int chapter=1;
  int chapterIndex=1;
  int section=1;
  int sectionIndex=1;
  std::vector<int> switchingPoint_chapter(1,-1);
  std::vector<int> switchingPoint_section(1,-1);
  FILE* inputFile_ptr;
  FILE* outputFile_ptr;
  std::string outputPath;
  std::ofstream  result;
  inputFile_ptr = fopen( argv[1], "rb");
  setvbuf(inputFile_ptr, NULL, _IOFBF, 1880);
  if(inputFile_ptr == NULL){                             //ファイルが無ければ終了
    std::ofstream errorLog( "error.txt", std::ios::app);
    std::cout << "read Error\n";
    errorLog << "read Error" << "\n" << "could not read the file.\n";
    errorLog.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    exit (1);
  }
  /*ファイルサイズ取得*/
  struct stat inputFile_stat;
  stat(argv[1], &inputFile_stat);
  fileSize = inputFile_stat.st_size;

  /*現時刻取得*/
  time_t rawTime = time(NULL);
  struct tm* dateAndTime;
  dateAndTime = localtime(&rawTime);
  const int dateAndTimeStrSize = sizeof("0000/00/00(***) 00:00:00");
  char dateAndTimeStr[dateAndTimeStrSize];
  strftime( dateAndTimeStr, dateAndTimeStrSize, "%Y/%m/%d(%a) %H:%M:%S", dateAndTime);

  result.open( "result.txt", std::ios::app );
  result << "### " << dateAndTimeStr << "\n### " << argv[1] << "\n### " << fileSize/1024 << "KB\n";
  /*
  ###
  ###読み込み
  ###
  */
  result << "==Read==\n";
  std::cout << "Reading\n";
  startTime = clock();
  while( packetNumber < (fileSize/188) ){          //ファイルの末端に到達で終了
    loadTsPacket( inputFile_ptr, tsPacketBytes, fileSize, packetNumber*188 );
    if((packetNumber%500)==0){
      progress = static_cast<double>(packetNumber*188)/fileSize;
      std::cout << "\r" << std::dec << std::setw(6) << std::setfill('0') << std::fixed << std::setprecision(2) << progress*100 << "%";
    }
    for(int i=0; i<10; i++){
      if( (packetNumber+i) < (fileSize/188) ){
        if( ((tsPacketBytes[1+(i*188)]*0x100+tsPacketBytes[2+(i*188)])&0x1FFF)==0 ){  //PID=0、つまりPATパケットのときtrue
          if(read_PAT_Info(tsPacketBytes, packetNumber, i, PMT_PID)){
            switchingPoint_section.push_back(packetNumber + i);
            std::cout << "\rPacket:" << std::setw(8) << std::setfill(' ') << packetNumber+i << "; [PMT_PID]" << previousPMT_PID << "->" << PMT_PID << "\n";
            result    << "Packet:" << std::setw(8) << std::setfill(' ') << packetNumber+i << "; [PMT_PID]" << previousPMT_PID << "->" << PMT_PID << "\n";
          }
          PAT = packetNumber + i;
        }
        previousPMT_PID=PMT_PID;
        if( checkPCR(tsPacketBytes, packetNumber, i, &PCR) ){
          switchingPoint_section.push_back(PAT);
          std::cout << "\rPacket:" << std::setw(8) << std::setfill(' ') << packetNumber+i << "; [PCR]" << std::setw(13) << previousPCR << "->" << std::setw(13) << PCR << "\n";
          result    << "Packet:" << std::setw(8) << std::setfill(' ') << packetNumber+i << "; [PCR]" << std::setw(13) << previousPCR << "->" << std::setw(13) << PCR << "\n";
        }
        previousPCR=PCR;
        if(search_Copyleft(tsPacketBytes, packetNumber, i) && ((packetNumber+i)>2)){
          switchingPoint_chapter.push_back(packetNumber + i - 2);
          std::cout << "\rPacket:" << std::setw(8) << std::setfill(' ') << packetNumber+i << "; [Copyleft]\n";
          result    << "Packet:" << std::setw(8) << std::setfill(' ') << packetNumber+i << "; [Copyleft]\n";
        }
      }
    }
    packetNumber=packetNumber+10;
  }
  std::cout << "\r100.00%\n";
  /*昇順にして重複削除*/
  std::sort(switchingPoint_section.begin(), switchingPoint_section.end());
  switchingPoint_section.erase(std::unique(switchingPoint_section.begin(), switchingPoint_section.end()), switchingPoint_section.end());
  std::sort(switchingPoint_chapter.begin(), switchingPoint_chapter.end());
  switchingPoint_chapter.erase(std::unique(switchingPoint_chapter.begin(), switchingPoint_chapter.end()), switchingPoint_chapter.end());

  result << "[Capter]\n";
  for (int i=1; i != switchingPoint_chapter.size(); i++){
    result << "  No." << std::setw(2) << i << "|";
    result << "Packet:" << std::setw(8) << std::setfill(' ') << switchingPoint_chapter[i] << "\n";
  }
  result << "[Section]\n";
  for (int i=1; i != switchingPoint_section.size(); i++){
    result << "  No." << std::setw(2) << i << "|";
    result << "Packet:" << std::setw(8) << std::setfill(' ') << switchingPoint_section[i] << "\n";
  }
  switchingPoint_chapter.push_back(-1);
  switchingPoint_section.push_back(-1);
  result << std::setw(7) << std::fixed << std::setprecision(2) << (static_cast<double>((clock()-startTime)))/1000  << "s\n";
  result << std::setw(6) << std::fixed << std::setprecision(2) << (static_cast<double>(fileSize/1024)) / ( static_cast<double>((clock()-startTime)/1000) ) << "KB/s\n";
  result.close();

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  /*
  ###
  ###書き込み
  ###
  */
  generateOutputPath(argv[1], chapter, section, &outputPath);
  outputFile_ptr = fopen( outputPath.c_str(), "wb");
  setvbuf(outputFile_ptr, NULL, _IOFBF, 1880);
  rewind(inputFile_ptr);
  result.open( "result.txt", std::ios::app );
  result << "==Write==\n";
  std::cout << "Writing\n";
  startTime=clock();
  packetNumber=0;
  while( packetNumber < (fileSize/188) ){
    loadTsPacket( inputFile_ptr, tsPacketBytes, fileSize, packetNumber*188);
    if((packetNumber%500)==0){
      progress = static_cast<double>(packetNumber*188)/fileSize;
      std::cout << "\r" << std::dec << std::setw(6) << std::setfill('0') << std::fixed << std::setprecision(2) << progress*100 << "%";
    }

    if( (10<(switchingPoint_chapter[chapterIndex]-packetNumber)) && (10<(switchingPoint_section[sectionIndex]-packetNumber)) && (10<(fileSize/188-packetNumber)) ){
    fwrite(tsPacketBytes, 1, 1880, outputFile_ptr);
    }else{
      for(int i=0; i<10; i++){
        if( fileSize/188 <= (packetNumber+i) ){   //末端到達でbreak
          break;
        }
        if( (packetNumber+i)==switchingPoint_chapter[chapterIndex] && (packetNumber+i)==switchingPoint_section[sectionIndex] ){         //切り替え地点に到達したとき
          fclose(outputFile_ptr); 
          chapterIndex=chapterIndex+1;
          sectionIndex=sectionIndex+1;
          chapter=chapter+1;
          section=1;
          generateOutputPath( argv[1], chapter, section, &outputPath);
          std::cout << "\r" << "Packet:" << std::setw(8) << std::setfill(' ') << packetNumber+i << " -> " << outputPath << "\n";
          result    << "Packet:" << std::setw(8) << std::setfill(' ') << packetNumber+i << " -> " << outputPath << "\n";
          outputFile_ptr = fopen( outputPath.c_str(), "wb");
          setvbuf(outputFile_ptr, NULL, _IOFBF, 1880);
        }else if( (packetNumber+i)==switchingPoint_chapter[chapterIndex]){          //切り替え地点に到達したとき
          fclose(outputFile_ptr);
          chapterIndex=chapterIndex+1;
          chapter=chapter+1;
          section=1;
          generateOutputPath( argv[1], chapter, section, &outputPath);
          std::cout << "\r" << "Packet:" << std::setw(8) << std::setfill(' ') << packetNumber+i << " -> " << outputPath << "\n";
          result    << "Packet:" << std::setw(8) << std::setfill(' ') << packetNumber+i << " -> " << outputPath << "\n";
          outputFile_ptr = fopen( outputPath.c_str(), "wb");
          setvbuf(outputFile_ptr, NULL, _IOFBF, 1880);
        }else if( (packetNumber+i)==switchingPoint_section[sectionIndex]){          //切り替え地点に到達したとき
          fclose(outputFile_ptr);
          sectionIndex=sectionIndex+1;
          section=section+1;
          generateOutputPath( argv[1], chapter, section, &outputPath);
          std::cout << "\r" << "Packet:" << std::setw(8) << std::setfill(' ') << packetNumber+i << " -> " << outputPath << "\n";
          result    << "Packet:" << std::setw(8) << std::setfill(' ') << packetNumber+i << " -> " << outputPath << "\n";
          outputFile_ptr = fopen( outputPath.c_str(), "wb");
          setvbuf(outputFile_ptr, NULL, _IOFBF, 1880);
        }
        fwrite(tsPacketBytes, 1, 188, outputFile_ptr);
        for(int j=0; j<(1880-188); j++){       //配列内のデータをずらす
          tsPacketBytes[j]=tsPacketBytes[j+188];
        }
      }
    }
    packetNumber=packetNumber+10;
  }
  std::cout << "\r100.00%\n";
  result << std::setw(7) << std::fixed << std::setprecision(2) << ( static_cast<double>((clock()-startTime)))/1000  << "s\n";
  result << std::setw(6) << std::fixed << std::setprecision(2) << (static_cast<double>(fileSize/1024)) / ( static_cast<double>((clock()-startTime)/1000) ) << "KB/s\n\n";
  result.close();
  fclose(outputFile_ptr);
  delete[] tsPacketBytes;
  return 0;
}