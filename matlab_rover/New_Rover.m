%%% INITIALIZATION OF CONNECTIONS
client = serial('COM7', 'BaudRate', 19200, 'Terminator', 'CR', 'Timeout', 300);
fopen(client);

%%% Communication Definitions
header1 = 2*16 + 11; % (0x2B)
header2 = 9*16 + 15; % (0x9F)
footer  = 5*16 + 12; % (0x5C)
legacy  = 10; % legacy
msg_len   = 0;
count   = 0;
command_f = 1; command_r = 47; command_l = 36; command_b = 4; command_s = 5;
wheels_data = 7; sensor_data = 11;
rsensor = 17; 
rov_msg_skel = [header1, header2, count, msg_len, 0, 0, 0, 0, 0, 0, 0, 0, 137, footer];
% 
% Rover to Arm messages
% [ HEADER1 ]	    [ HEADER2 ]	          [ COUNT ]      [length]      [   DATA    ]  [ FOOTER     ]
% |----1 byte----|	|----1 byte----|	|--1 byte--|    |--1 byte--|   |--9 bytes--|  |---1 byte---|

%%% INITIALIZATION OF MAP
c_map = zeros(1000, 1000);

[r, c] = size(c_map);
for i = 1:r
    for j = 1:c
        if ((i == 200 || i == 800) && j >= 200 && j <= 800)
            c_map(i,j) = 1;
        end
        if ((j == 200 || j == 800) && i >= 200 && i <= 800)
            c_map(i,j) = 1;
        end
    end
end
% Initial starting positions on both sides.
orientation = 0;
c_pos = [750, 500];
for i = -2:2
    c_map(c_pos(1) + i, c_pos(2)) = 1;
end

% Controls whether a display is shown of the map.
show_physical_map = 1;

if (show_physical_map)
    spy(c_map);
    pause(2);
end

% WARNING. THIS IS NOT VALID IF NEW DAY BEGINS DURING RUNTIME.
clock_scales = [0, 0, 0, 3600, 60, 1];

% Rover moves at 20 inches per second. 41 inches wide, 61 inches long.
r_speed = 40;
r_wid = 41;
r_len = 61;
r_offs = [(r_wid-1)/2, (r_len-1)/2];

%%% Sensor offsets depending on orientation.
u_off_0 = [-r_offs(1), r_offs(2); r_offs(1), r_offs(2); r_offs(1), r_offs(2); r_offs(1), -r_offs(2)];
u_off_1 = [-r_offs(2), -r_offs(1); -r_offs(2), r_offs(1); -r_offs(2), r_offs(1); r_offs(2), r_offs(1)];
u_off_2 = u_off_0 * -1;
u_off_3 = u_off_1 * -1;
% u_offs  = [-r_offs(1), r_offs(2); r_offs(1), r_offs(2); r_offs(1), r_offs(2); r_offs(1), -r_offs(2);
%            r_offs(2), r_offs(1); r_offs(2), -r_offs(1); r_offs(2), -r_offs(1); -r_offs(2), -r_offs(1);
%            0,0; 0,0; 0,0; 0,0; 0,0; 0,0; 0,0; 0,0];
% u_offs(9:12,  :) = u_offs(1:4, :) * -1;
% u_offs(13:16, :) = u_offs(5:8, :) * -1;
o_sense = [1,0; 1,0; 0,-1; 0,-1; -1,0; -1,0; 0,1; 0,1; 1,0; 1,0];



%%% Runtime loop
moving = 0;
summed_movement = 0;
while 1
    % Read message from arm
    inc_msg = fread(client, 5)';

    % Update Rover Location.
    if (moving ~= 0)
        % Erase old rover location.
        if (show_physical_map)
            for i = -2:2
                c_map(c_pos(1) + i, c_pos(2)) = 0;
            end
        end
        
        % Calculate time passed since last check. Invalid if day changes.
        % Use this time to calculate changes to rover location on the map.
        time = dot(clock_scales, (clock - started_time));
        movement = round(time * r_speed * moving);
        o_move = [0,movement; -movement,0; 0,-movement; movement,0];
        c_pos = c_pos + o_move(orientation+1, :);
        summed_movement = summed_movement + movement;
        
        % Draw new rover location.
        if (show_physical_map)
            for i = -2:2
                c_map(c_pos(1) + i, c_pos(2)) = 1;
            end
            spy(c_map);
            pause(0.01);
        end
    end
    started_time = clock;
    
    % Start preparing return message.
    out_msg = rov_msg_skel;
    out_msg(1, 3) = inc_msg(1,4);
    
    % Branch based on request type.
    if (inc_msg(1,3) == rsensor)
        inc_msg(1,3) = sensor_data;
    end
    switch(inc_msg(1,3))
        
        case command_f
            fprintf('Received go request!\n');
            moving = 1;
            
        case command_b
            moving = -1;
            
        case command_s
            moving = 0;
            
        case command_l
            orientation = orientation + 1;
            if (orientation == 4)
                orientation = 0;
            end
            summed_movement = 0;
            
        case command_r
            orientation = orientation - 1;
            if (orientation == -1)
                orientation = 3;
            end
            summed_movement = 0;
            
        case wheels_data
            if (summed_movement > 255)
                out_msg(1,4) = 2; % two useful bytes.
                out_msg(1,5) = summed_movement / 256;
                out_msg(1,6) = rem(summed_movement, 256);
            else
                out_msg(1,4) = 1; % one useful byte.
                out_msg(1,5) = summed_movement;
            end
%             out_msg(1,5) = 10;
%             out_msg(1,6) = 254;
%             out_msg(1,7) = 254;
%             out_msg(1,8) = 254;
%             out_msg(1,9) = 254;
%             out_msg(1,10) = 254;
%             out_msg(1,11) = 254;
%             out_msg(1,12) = 254;
%             out_msg(1,13) = 254;
            
            fprintf('Wheel return: %i\n', out_msg(1,5));
            summed_movement = 0;
            
        case sensor_data
            data = [255, 255, 255, 255];
            sensors = [c_pos; c_pos; c_pos; c_pos];
            switch(orientation)
                case 0
                    sensors = sensors + u_off_0;
                case 1
                    sensors = sensors + u_off_1;
                case 2
                    sensors = sensors + u_off_2;
                case 3
                    sensors = sensors + u_off_3;
            end
            for i = 1:4
                for j = 1:254
                    if (data(1, i)==255 && c_map(sensors(i,1)+o_sense(2*(3-orientation)+i,1)*j, sensors(i,2)+o_sense(2*(3-orientation)+i,2)*j) == 1)
                        data(1, i) = j;
                        break;
                    end
                end
            end
            out_msg(1, 4) = 2;
            out_msg(1, 5:8) = data;
            fprintf('Sensor data: %i %i %i %i\n', out_msg(1,5), out_msg(1,6), out_msg(1,7), out_msg(1,8));
    end
    fwrite(client, out_msg);
end

%%% CLEANUP
fclose(client);