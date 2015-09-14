/* Taxonomy Classification: 0100000000000000000300 */

/*
 *  WRITE/READ               	 0	write
 *  WHICH BOUND              	 1	lower
 *  DATA TYPE                	 0	char
 *  MEMORY LOCATION          	 0	stack
 *  SCOPE                    	 0	same
 *  CONTAINER                	 0	no
 *  POINTER                  	 0	no
 *  INDEX COMPLEXITY         	 0	constant
 *  ADDRESS COMPLEXITY       	 0	constant
 *  LENGTH COMPLEXITY        	 0	N/A
 *  ADDRESS ALIAS            	 0	none
 *  INDEX ALIAS              	 0	none
 *  LOCAL CONTROL FLOW       	 0	none
 *  SECONDARY CONTROL FLOW   	 0	none
 *  LOOP STRUCTURE           	 0	no
 *  LOOP COMPLEXITY          	 0	N/A
 *  ASYNCHRONY               	 0	no
 *  TAINT                    	 0	no
 *  RUNTIME ENV. DEPENDENCE  	 0	no
 *  MAGNITUDE                	 3	4096 bytes
 *  CONTINUOUS/DISCRETE      	 0	discrete
 *  SIGNEDNESS               	 0	no
 */

/*
Copyright 2005 Massachusetts Institute of Technology
             All rights reserved. 

Redistribution and use of software in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met.

    - Redistributions of source code must retain the above copyright notice, 
      this set of conditions and the disclaimer below.
    - Redistributions in binary form must reproduce the copyright notice, this 
      set of conditions, and the disclaimer below in the documentation and/or 
      other materials provided with the distribution.
    - Neither the name of the Massachusetts Institute of Technology nor the 
      names of its contributors may be used to endorse or promote products 
      derived from this software without specific prior written permission. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS".

ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. 

IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
*/


int main(int argc, char *argv[])
{
  char buf[10];


  /*  BAD  */
  buf[-4096] = 'A';


  return 0;
}
