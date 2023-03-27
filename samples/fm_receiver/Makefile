
fm_receiver: fm_receiver.c
	gcc -Wall -g -o fm_receiver fm_receiver.c -l fftw3 -l m -l rtlsdr

clean:
	rm fm_reciever

run: fm_receiver
	./fm_receiver  | aplay -r 20000 -t raw -f S16_LE

