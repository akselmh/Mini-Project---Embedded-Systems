----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 09/02/2025 09:18:02 AM
-- Design Name: 
-- Module Name: comparator - Behavioral
-- Project Name: 
-- Target Devices: 
-- Tool Versions: 
-- Description: 
-- 
-- Dependencies: 
-- 
-- Revision:
-- Revision 0.01 - File Created
-- Additional Comments:
-- 
----------------------------------------------------------------------------------


library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity comparator is
    generic(
        n_bits  :   POSITIVE    :=  4
    );

    Port ( in1 : in STD_LOGIC_VECTOR (n_bits-1 downto 0);
           in2 : in STD_LOGIC_VECTOR (n_bits-1 downto 0);
           out1 : out STD_LOGIC);
end comparator;


architecture Behavioral of comparator is
begin
    process(in1,in2)
    begin
        if(in1<=in2)then 
            out1<='1';
        else
            out1<='0';
        end if;
    end process;        
end Behavioral;