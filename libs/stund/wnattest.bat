@echo off
rem wnattest.bat
if defined %4 (
	goto usage
) else if defined %3 (
	goto start
) else (
	goto usage
)
:start
stunner %1 -i %2 -i2 %3
if %errorlevel% == -1 (
	echo "ERROR! the STUN test program had an error" (
) else if %errorlevel% == 10 (
	echo "[PASS] (Address) Restricted Cone NAT with Hairpinning"
) else if %errorlevel% == 21 (
	echo "[PASS] Port Restricted Cone NAT with Hairpinning"
) else if %errorlevel% == 8 (
	echo "[No NAT] You have open internet access"
) else if %errorlevel% == 2 (
	echo "[FAIL] Your (Address) Restricted Cone NAT doesn't do hairpinning"
) else if %errorlevel% == 3 (
	echo "[FAIL] Your Port Restricted Cone NAT doesn't do hairpinning"
) else (
	echo "[FAIL] You have a NAT or Firewall type which is NOT RECOMMENDED.  "
	if %errorlevel% LSS 8 (
		echo "It also does not support hairpinning"
	) else (
		each "it does at least support hairpinning"
	)	  
) 
goto end
:usage
echo Usage:
echo         wnattest <server-ip> <client-primary-ip> <client-secondary-ip>
echo.
echo Example: wnattest 1.1.1.2 192.168.0.2 192.168.0.3
echo.
:end

