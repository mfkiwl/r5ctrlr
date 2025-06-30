
%fname='sine4.txt';
fname='sweep4.txt';
%ch=1;
ch=4;

samples=readmatrix(fname,'NumHeaderLines',1);

figure(1);
plot(samples(:,ch));




