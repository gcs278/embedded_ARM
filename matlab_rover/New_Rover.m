%%% INITIALIZATION OF CONNECTIONS
client = serial('COM9', 'BaudRate', 19200, 'Terminator', 'CR', 'Timeout', 300);
fopen(client);
oldclient = client;

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
running_movement_total = 0;
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
        
        % Add a protrusion!
        if (i == 800 && j >= 450 && j <= 550)
            c_map(i,j) = 0;
        end
        if ((i == 700 || i == 800) && j >= 450 && j <= 550)
            c_map(i,j) = 1;
        end
        if ((j == 450 || j == 550) && i >= 700 && i <= 800)
            c_map(i,j) = 1;
        end
    end
end


% Initial starting positions on both sides.
orientation = 0;
c_pos = [770, 250];

% Rover moves at 20 inches per second. 41 inches wide, 41 inches long.
r_speed = 10;
r_wid = 41;
r_len = 41;
r_offs = [(r_wid-1)/2, (r_len-1)/2];

%%% Sensor offsets depending on orientation.
u_offs  = [-r_offs(1), r_offs(2); r_offs(1), r_offs(2); r_offs(1), r_offs(2); r_offs(1), -r_offs(2);
           -r_offs(2), -r_offs(1); -r_offs(2), r_offs(1); -r_offs(2), r_offs(1); r_offs(2), r_offs(1);
           0,0; 0,0; 0,0; 0,0;
           0,0; 0,0; 0,0; 0,0];
u_offs(9:12,  :) = u_offs(1:4, :) * -1;
u_offs(13:16, :) = u_offs(5:8, :) * -1;
o_sense = [1,0; 1,0; 0,-1; 0,-1; -1,0; -1,0; 0,1; 0,1; 1,0; 1,0];

% Controls whether a display is shown of the map.
show_physical_map = 1;
if (show_physical_map)
    sensors = [c_pos; c_pos; c_pos; c_pos] + u_offs(4*orientation+1:4*orientation+4, :);
    for i = c_pos(1)-r_offs(1):c_pos(1)+r_offs(1)
        c_map(i, c_pos(2)+r_offs(2)) = 1;
        c_map(i, c_pos(2)-r_offs(2)) = 1;
    end
    for j = c_pos(2)-r_offs(2):c_pos(2)+r_offs(2)
        c_map(c_pos(1)+r_offs(1), j) = 1;
        c_map(c_pos(1)-r_offs(1), j) = 1;
    end
    spy(c_map);
    pause(2);
end

% WARNING. THIS IS NOT VALID IF NEW DAY BEGINS DURING RUNTIME.
clock_scales = [0, 0, 0, 3600, 60, 1];

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
            sensors = [c_pos; c_pos; c_pos; c_pos] + u_offs(4*orientation+1:4*orientation+4, :);
            for i = c_pos(1)-r_offs(1):c_pos(1)+r_offs(1)
                c_map(i, c_pos(2)+r_offs(2)) = 0;
                c_map(i, c_pos(2)-r_offs(2)) = 0;
            end
            for j = c_pos(2)-r_offs(2):c_pos(2)+r_offs(2)
                c_map(c_pos(1)+r_offs(1), j) = 0;
                c_map(c_pos(1)-r_offs(1), j) = 0;
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
            sensors = [c_pos; c_pos; c_pos; c_pos] + u_offs(4*orientation+1:4*orientation+4, :);
            for i = c_pos(1)-r_offs(1):c_pos(1)+r_offs(1)
                c_map(i, c_pos(2)+r_offs(2)) = 1;
                c_map(i, c_pos(2)-r_offs(2)) = 1;
            end
            for j = c_pos(2)-r_offs(2):c_pos(2)+r_offs(2)
                c_map(c_pos(1)+r_offs(1), j) = 1;
                c_map(c_pos(1)-r_offs(1), j) = 1;
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
    if (inc_msg(1,3) == 2)
        inc_msg(1,3) = command_r;
        fprintf('received legacy right\n');
    end
    if (inc_msg(1,3) == 3)
        inc_msg(1,3) = command_l;
        fprintf('received legacy left\n');

    end
    switch(inc_msg(1,3))
        
        case command_f
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
            fprintf('Turn left!\n');
        case command_r
            orientation = orientation - 1;
            if (orientation == -1)
                orientation = 3;
            end
            summed_movement = 0;
            fprintf('Turn right!\n');
        case wheels_data4
            if (summed_movement > 255)
                out_msg(1,4) = 2; % two useful bytes.
                out_msg(1,5) = summed_movement / 256;
                out_msg(1,6) = rem(summed_movement, 256);
            else
                out_msg(1,4) = 1; % one useful byte.
                out_msg(1,5) = summed_movement;
            end
            running_movement_total = running_movement_total + summed_movement;
            summed_movement = 0;
            fprintf('wheel data!\n');
            
        case sensor_data
            data = [255, 255, 255, 255];
            sensors = [c_pos; c_pos; c_pos; c_pos] + u_offs(4*orientation+1:4*orientation+4, :);
            for i = 1:4
                for j = 1:254
                    if (c_map(sensors(i,1)+o_sense(2*(3-orientation)+i,1)*j, sensors(i,2)+o_sense(2*(3-orientation)+i,2)*j) == 1)
                        data(1, i) = j;
                        break;
                    end
                end
            end
            out_msg(1, 4) = 2;
            out_msg(1, 5:8) = data;
            fprintf('Sensor data: %i %i %i %i cpos: %i %i\n', out_msg(1,5), out_msg(1,6), out_msg(1,7), out_msg(1,8), c_pos(1,1), c_pos(1,2));
        
    end
    fwrite(client, out_msg);
end

%%% CLEANUP
fclose(client);