#include <math.h>

#include "draw.hh"


/**
 *      The function graph_std() plots the input parameters to a pdf file.
 *      The parameter "group" needs to have a specific layout:
 *      It has to contain at least 1 ndmapSet. Each ndmapSet needs to have exactly three ndmaps (map):
 *      At position map[0] = upper limit of trajectory (trajectory + standard deviation);
 *      at postiton map[1] = trajectory;
 *      and at pos. map[2] = nower limit of trajectory (trajectory - standard deviation);
 *      The goup - name is used for naming the pdf - file.
 *      The resulting pdf file is being saved in the directory this program is executed from.
 *
 */
int draw::graph_std(ndmapSetGroup group){		
  

  
  if( group.group_is_consistent() <= 0) return -1;      //   
        
  std::string dir("tmp/");               // Create a temporary folder for the provisional result
  std::string script("plot.txt");
  std::string leatra_output_ending("_apx");
  std::string latex;
  latex = group.get_name();
  latex += leatra_output_ending;
  latex += ".tex";
  std::string command = "mkdir " + dir;
  
  if(0 != system(command.c_str())) throw data_error(dir, -41);   
  
  
  litera lit;                           // Write each map to a file
  lit.write_all(group, dir);

  group_gnuscript(group, dir, script);  // Creating a gnuscript for the graphs of all dimensions
     
  command = "gnuplot " + dir + script;         // Creating the different graphs   
  if(0 != system(command.c_str())) throw data_error(command, -42);

  group_latex(group, dir, latex);              // Creating a latex script 
                      
  command = "pdflatex " + dir + latex + " > /dev/null ";
  if(0 != system(command.c_str())) throw data_error(command, -42); 

  latex = group.get_name();
  latex += leatra_output_ending;
  
  command = "rm -r ";
  command += dir;
  command += " " + latex + ".aux " + latex + ".log ";
  
  
  if(0 != system(command.c_str())) throw data_error(command, -42);                // Delete provisional results
  return 0;
}




int draw::graph_all(ndmapSetGroup group){
    
  if( group.group_is_consistent() <= 0) return -1;      //   
        
  std::string dir("tmp/");               // Create a temporary folder for the provisional result
  std::string script("plot.txt");         // for the gnuplotfile
  std::string leatra_output_ending("_apx"); // for the pdf (latex)
  std::string latex;
  latex = group.get_name();
  latex += leatra_output_ending;
  latex += ".tex";
  std::string command = "mkdir " + dir;
  
  if(0 != system(command.c_str())) throw data_error(dir, -41);   
  
  
  litera lit;                           // Write each map to a file
  lit.write_all(group, dir);

  group_gnuscript(group, dir, script);  // Creating a gnuscript for the graphs of all dimensions
     
  command = "gnuplot " + dir + script;         // Creating the different graphs   
  if(0 != system(command.c_str())) throw data_error(command, -42);

  group_latex(group, dir, latex);              // Creating a latex script 
                      
  command = "pdflatex " + dir + latex + " > /dev/null ";
  if(0 != system(command.c_str())) throw data_error(command, -42); 

  latex = group.get_name();
  latex += leatra_output_ending;
  
  command = "rm -r ";
  command += dir;
  command += " " + latex + ".aux " + latex + ".log ";
  
  
  if(0 != system(command.c_str())) throw data_error(command, -42);                // Delete provisional results
  return 0;
    
    
}



/**
 *      The method group_latex writes the latex script in order to produce the pdf "output" script.
 */
int draw::group_latex(ndmapSetGroup group, std::string dir, std::string latex){

  std::string file = dir + latex;
  int dim = group.get_dim();
    
  std::ofstream wFile(file.c_str());
    
  if (wFile.is_open()) {

    wFile<<"\\documentclass[a4paper, 10pt]{article} \n";
    wFile<<"\\usepackage{graphicx} \n"; 
    wFile<<"\\setlength{\\oddsidemargin}{-1.5cm} \n"; 
    wFile<<"\\setlength{\\evensidemargin}{-1.5cm} \n";
    wFile<<"\\setlength{\\textwidth}{18cm} \n";
    wFile<<"\\begin{document} \n";
    wFile<<"\\title{"<<group.get_name()<<"} \n";
    wFile<<"\\author{Leatra (Benjamin Reiner)} \n";
    wFile<<"\\maketitle \n";
    wFile<<"\\begin{center} \n \\  \\\\ \nThis document is generated by Leatra.  \\\\  \n It contains the graphs from: "<< group.get_name() <<". \\\\ \n \\  \\\\ \n \\  \\\\ \n \\end{center} \n";   
    
    for(int d = 0; d < dim; d++){
      wFile<<"\\begin{flushleft} \n Dimension "<< d <<": \n \\end{flushleft} \n";   
      wFile<<"\\includegraphics[scale=0.47]{"<< dir <<"d"<< d <<".pdf} \n \n";
    }
    wFile<<"\\end{document} \n";
    wFile.close();
    return 0;
  }
  return -1;			//Error: File couldn't be opened.  
}


/**
 *      The method group_gnuscript() writes the script for gnuplot in order to create the graphs.
 */
int draw::group_gnuscript(ndmapSetGroup group, std::string dir, std::string script){
    
  std::string colors_strong[4] = {"#FF0000", "#0000FF", "#00FF00", "#FF8000" };
  std::string colors_soft[5] = {"#FF8080", "#8080FF", "#80FF80",  "#FFBF80" };

  int color_count;
  
  std::string file = dir + script;
  int dim = group.get_dim();
    
  std::ofstream wFile(file.c_str());
    
  if (wFile.is_open()) {

    wFile<<"set term pdfcairo enhanced font 'arial,10' size 15, 5\n";
    wFile<<"set style fill transparent solid 0.30 noborder\n"; 
    wFile<<"set key left top vertical Right enhanced autotitles nobox\n";
   // wFile<<"set title \" Dimension "<< d <<"\" "\n";

    for(int d = 0; d < dim; d++){
        wFile<<"set title \" Dimension "<< d <<"\" \n";
      color_count = 4;   
    
      file = dir + script + intTOstring( d );    
      wFile<<"set output '"<< dir <<"d"<< d << ".pdf'\n";
      wFile<<"plot ";  
    
      int sets = group.get_num_of_sets();
          
      for(int s = 0; s < sets ; s++){
        
        wFile<<"'"<< dir <<"d"<< d <<"s"<< s <<"'";
        //wFile<<" u 0:1:3 w filledcurves above lt 1 , ";
        wFile<<" u 0:1:3 w filledcurves above lt 1 lc rgb \"" << colors_soft[ color_count % 4 ] << "\"  title 'standard deviation of "<< group.get_set_name(s) <<"' , ";
        wFile<<" '' u 0:2 with lines lc rgb \""<< colors_strong[ color_count % 4 ] << "\"  title '"<< group.get_set_name(s)  <<"'   " ;
        //wFile<<" '' u 0:2 with lines ";
        if(s+1 < sets) wFile<<" , ";
        color_count++;
      }      
      wFile<<"\n set output \n";  
    }
    
    wFile.close();
    return 0;
  }
  return -1;			//Error: File couldn't be opened.  
}