


***To build quash, navigate to the directory "quash" which contains the source files and run "make all":**


                make all

This will compile the quash source code and generate the executable file.

***Running Quash***

To start quash, run the following command:


      ./quash
      
      
 the quash directory contaisn a.txt and b .txt for your testing purposes feel free to use them to test I/O redirection.
 
 **sometimes if quash gets stuck press enter to clear stuff up**
 
 __list of commands to try :__
 
echo hello 
 
cd src
  
pwd

cd ..
 
ls 

echo $PWD

sleep 30 &

jobs 

sleep 3

jobs

kill %<JOBID>

kill <PID>

please do note that even after termination the jobs command will show as completed not as terminated 

grep "this" a.txt
 
cat < a.txt
 
cat a.txt >> b.txt


ls | grep c 

cat src/quash.c | grep QUASH


please do note that sometimes grep expects the search pattern argument without quotes 
