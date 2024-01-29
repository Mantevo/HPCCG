#include <ctime>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#ifdef REDSTORM
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include "YAML_Doc.hpp"
using namespace std;

/**
 * Set the mini-app name and version which will become part of the YAML doc.
 *
 * @param miniApp_Name The name of the mini-app.
 * @param miniApp_Version The version of the mini-app.
 * @param destination_Directory The output directory for the YAML file.
 * @param destination_FileName The output filename for the YAML file.
 */
YAML_Doc::YAML_Doc(const std::string& miniApp_Name, const std::string& miniApp_Version, const std::string& destination_Directory, const std::string& destination_FileName){
  miniAppName = miniApp_Name;
  miniAppVersion = miniApp_Version;
  destinationDirectory = destination_Directory;
  destinationFileName = destination_FileName;
}

/**
 * Inherits the destructor from YAML_Element.
 */
YAML_Doc::~YAML_Doc(void){
}

/**
 * Generate YAML from the elements of the document and save it to a file.
 *
 * @return A string containing the YAML data written to the file.
 */
string YAML_Doc::generateYAML(){
  string yaml;
  yaml =  yaml + "Mini-Application Name: " + miniAppName + "\n";
  yaml =  yaml + "Mini-Application Version: " + miniAppVersion + "\n";
  for(size_t i=0; i<children.size(); i++){
    yaml = yaml + children[i]->printYAML("");
  }
  
  time_t rawtime;
  tm * ptm;
  time ( &rawtime );
  ptm = localtime(&rawtime);
  char sdate[25];
  //use tm_mon+1 because tm_mon is 0 .. 11 instead of 1 .. 12
  sprintf (sdate,"%04d_%02d_%02d__%02d_%02d_%02d",ptm->tm_year + 1900, ptm->tm_mon+1,
    ptm->tm_mday, ptm->tm_hour, ptm->tm_min,ptm->tm_sec);

  string filename;
  if (destinationFileName=="") 
    filename = miniAppName + "-" + miniAppVersion + "_";
  else 
    filename = destinationFileName;
  filename = filename + string(sdate) + ".yaml";
  if (destinationDirectory!="" && destinationDirectory!=".") {
    string mkdir_cmd = "mkdir " + destinationDirectory;
#ifdef REDSTORM
    mkdir(destinationDirectory.c_str(),0755);
#else
    system(mkdir_cmd.c_str());
#endif
    filename = destinationDirectory + "/" + destinationFileName;
  }
  else 
    filename = "./" + filename;

  ofstream myfile;
  myfile.open(filename.c_str());
  myfile << yaml;
  myfile.close();
  return yaml;
}
