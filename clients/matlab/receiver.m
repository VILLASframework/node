% Simple MATLAB code to receive VILLAS UDP samples
%
% @author Megha Gupta <meghagupta1191@gmail.com>
% @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
% @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
% @license GNU General Public License (version 3)
%
% VILLASnode
%
% This program is free software: you can redistribute it and/or modify
% it under the terms of the GNU General Public License as published by
% the Free Software Foundation, either version 3 of the License, or
% any later version.
%
% This program is distributed in the hope that it will be useful,
% but WITHOUT ANY WARRANTY; without even the implied warranty of
% MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
% GNU General Public License for more details.
%
% You should have received a copy of the GNU General Public License
% along with this program.  If not, see <http://www.gnu.org/licenses/>.
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

close all;
clear all;
clc;

% Configuration and connection
t = udp('127.0.0.1',                ...
    'LocalPort', 12000,             ...
    'RemotePort', 12001,            ...
    'Timeout', 60,                  ...
    'DatagramTerminateMode', 'on' ...
);
disp('UDP Receiver started');

fopen(t);
disp('UDP Socket bound');

num_values = 5;
num_samples = 500;

data = zeros(num_values, num_samples);
i = 1;

disp('Receiving data');
while i < num_samples
    % Wait for connection
    % Read data from the socket
    
    [ dat, count ] = fread(t, num_values, 'float32');
    
    data(:,i) = dat;
    i = i + 1;
end

plot(data');

fclose(t);
delete(t);
