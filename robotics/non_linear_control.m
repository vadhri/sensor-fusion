R = [1 0 0; 0 -1 0; 0 0 -1]; %2
R = [0.5 -1/sqrt(2) 0.5; 0.5 1/sqrt(2) 0.5; -1/sqrt(2) 0 1/sqrt(2)]; %0.5429
R = [0.75 -0.433 0.5; 0.5 0.866 0; -0.433 0.25 0.866] %0.2590
R = [0.5 0 0.866; 0 1 0; -0.866 0 0.5] %0.5

R

0.5*(trace(eye(3) - R))