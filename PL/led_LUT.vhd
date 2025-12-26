library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity led_LUT is
    Port (
        clk     : in  std_logic;
        rst     : in  std_logic;
        res   : in  std_logic_vector(31 downto 0);
        pwm     : out std_logic_vector(3 downto 0)
    );
end led_LUT;

architecture Behavioral of led_LUT is

    signal pred     : integer := 0;
    signal led_ctrl : std_logic_vector(3 downto 0);

begin


    --------------------------------------------------------------------
    -- Convert to prediction index
    --------------------------------------------------------------------
    pred <= to_integer(unsigned(res));

    --------------------------------------------------------------------
    -- LUT (same mapping you already had)
    --------------------------------------------------------------------
    with pred select led_ctrl <=
        "0011" when 1,
        "0101" when 2,
        "0111" when 3,
        "1001" when 4,
        "1011" when 5,
        "1111" when 6,
        "0000" when 0,
        "1111" when others;

    pwm <= led_ctrl;

end Behavioral;
