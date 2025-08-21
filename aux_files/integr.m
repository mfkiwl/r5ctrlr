close all
clear all

f0=1;
w0=2*pi*f0;
fvec=logspace(-1,+1,200);
labels=[];
figure(4)
H=tf([1], [1./w0 0]);
[mag,ph,wvec]=bode(H,fvec*2*pi);
fnvec=(wvec./(2.*pi));
magvec(:)=20.*log10(mag(1,1,:));
magvec=magvec';
%phvec(:)=180./pi*ph(1,1,:);
phvec(:)=ph(1,1,:);
phvec=phvec';
subplot(2,1,1);
semilogx(fnvec,magvec);
hold on;
grid on;
ylabel('Mag (dB)');
xlabel('f/f_0')
subplot(2,1,2);
semilogx(fnvec,phvec);
hold on;
grid on;
%ylim([-180 0]);
yticks([-180 -135 -90 -45 0])
ylabel('Phase (deg)');
xlabel('f/f_0')
subplot(2,1,1);
