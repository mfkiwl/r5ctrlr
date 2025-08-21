close all
clear all

% fz=1;
% fp=10;
fz=10;
fp=1;
wz=2*pi*fz;
wp=2*pi*fp;
fmin=min(fp,fz);
fmax=max(fp,fz);
fvec=logspace(log10(fmin)-1,log10(fmax)+1,200);
figure(4)
H=tf([1./wz 1.], [1./wp 1.]);
[mag,ph,wvec]=bode(H,fvec*2*pi);
fnvec=(wvec./(2.*pi));
magvec(:)=20.*log10(mag(1,1,:));
magvec=magvec';
%phvec(:)=180./pi*ph(1,1,:);
phvec(:)=ph(1,1,:);
phvec=phvec';
subplot(2,1,1);
semilogx(fnvec,magvec);
title(sprintf('Phase lead-lag; fz=%.0f, fp=%.0f',fz,fp))
hold on;
grid on;
ylabel('Mag (dB)');
xlabel('f (Hz)')
subplot(2,1,2);
semilogx(fnvec,phvec);
hold on;
grid on;
%ylim([-180 0]);
yticks([-180 -135 -90 -45 0 45 90])
ylabel('Phase (deg)');
xlabel('f (Hz)')
subplot(2,1,1);
