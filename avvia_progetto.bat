@echo off
REM Script di avvio rapido per il progetto Forza4.
REM Metti questo file nella cartella principale del progetto
REM (quella dove si trova docker-compose.yml).

pushd "%~dp0"

echo ============================================
echo  Avvio server Forza4 (Docker)...
echo ============================================
start "Server Forza4" cmd /k "docker compose up"

echo Attendo qualche secondo che il server sia pronto...
timeout /t 8 /nobreak >nul

echo ============================================
echo  Avvio client 1...
echo ============================================
start "Client Forza4 - 1" cmd /k "cd client && java -jar target\forza4-client-1.0.jar"

timeout /t 2 /nobreak >nul

echo ============================================
echo  Avvio client 2...
echo ============================================
start "Client Forza4 - 2" cmd /k "cd client && java -jar target\forza4-client-1.0.jar"

popd

echo.
echo Fatto. Si sono aperte 3 finestre: server + 2 client.
echo Per aprire un terzo client, rilancia semplicemente:
echo   cd client ^&^& java -jar target\forza4-client-1.0.jar
echo.
pause
