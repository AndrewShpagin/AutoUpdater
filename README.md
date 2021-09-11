# AutoUpdater / CloudUploader
This app intended to solve the fast delivery of updates to the final customer. 
## The task:

- Let we done some software that is updated often, usually on everyday basis.
- You need to deliver the updates to the user, but often downloading of the massive installer is not good idea.
- So we need sort of incremental updater, it downloads and installs only changed files.
- You need sort of UI to manage what build should be installed, you need to give possibility to users to switch between different builds.

## What we offer as solution
- This project consists of 2 parts: uploader and the client.
- When you change your software files and making new build, you are using uploader to upload changed files to the amazon aws bucket (it detects what was changed and zips and uploads only new/changed files).
- You making the installer as you was doing it before, but bringing the AutoUpdater with the distributive. When user need an update or switch to the other build, he presses the button in your program and it closes and calls AutoUpdater. 
- It opens in browser and offers the builds list to user. 
- User selects the build, AutoUpdater downloads changes, replaces the required files, removes the unnecessary ones.
- If user needs, he may run the software right from the page in browser.
