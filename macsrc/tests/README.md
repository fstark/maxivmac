The smoke tests for the maxivmac INIT

First, we take the init from ../init/maxivmac INIT and we inject it into two hfs disks, one 608, one 701. For this, we use the ad2bin tool and the hcopy command.

We then create a file with the build number in shared/BUILD.txt

Then, we launch 4 macs in a row, plus and II with 608 and 701.


We mount a test shared drive and open a file from within, checking that 
