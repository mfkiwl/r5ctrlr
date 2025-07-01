
% fname='bode1_1khz_500_3000_fs10k.txt';
% Fstart=500;
% Fstop=3000;
% Fs=10e3;

% fname='bode2_1khz_100_3000_fs10k';
% Fstart=100;
% Fstop=3000;
% Fs=10e3;

% fname='bode3_88hz_20_400_fs10k.txt';
% Fstart=20;
% Fstop=400;
% Fs=10e3;

fname='bode4_88hz_10_400_fs1k.txt';
Fstart=10;
Fstop=400;
Fs=1e3;


in_ch=4;
out_ch=1;
ADCmax=32768;

samples=readmatrix(fname,'NumHeaderLines',1);
[Ns Nch]=size(samples);

T=1/Fs;
df=Fs/Ns;
freqv=0:df:Fs/2-df;
timev=(0:Ns-1)*T;
irange=find((freqv>=Fstart) & (freqv<=Fstop));

vin = samples(:,in_ch)/ADCmax;
vout=samples(:,out_ch)/ADCmax;

figure(1);

subplot(2,1,1);
plot(timev,vin);
grid on;
title("input");
xlabel("time(s)")

subplot(2,1,2);
plot(timev,vout);
grid on;
title("output");
xlabel("time(s)")


%fin=fftshift(fft(vin));
fin=fft(vin);
fout=fft(vout);
h=fout./fin;

figure(2);

subplot(2,1,1);
plot(freqv,20*log10(abs(fin(1:Ns/2))));
grid on;
title("input");
xlabel("freq(Hz)")
ylabel("dB");

subplot(2,1,2);
plot(freqv,20*log10(abs(fout(1:Ns/2))));
grid on;
title("output");
xlabel("freq(Hz)")
ylabel("dB");

figure(3);
%semilogx(freqv,20*log10(abs(h(1:Ns/2))));
semilogx(freqv(irange),20*log10(abs(h((irange)))));
grid on
title("bode");
xlabel("freq(Hz)")
ylabel("dB");

