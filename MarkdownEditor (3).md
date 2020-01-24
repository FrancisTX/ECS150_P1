## ECS150 Program Report                  
### Running the Simple commands
In this step, we implement the simple commands using `fork()`, `wait()` and `exec()`. Before we execute the commands, we will use `fork()` to produce a child process and execute the command in the child process. Until the child process is done, the parent process will be executed because we use `wait()` for waiting child. Then in the parent process, we use `execvp()` to execute the command we want. `execvp()` has two parameters, the `$PATH` and arguments array one. It will search this path and then execute the arguments after that. That is the things we want. 


### Arguments
Next, we need to figure out how to handle the arguments. We have to parsing the command line. Our paring strategy is list below.
+ if

### Built-in Command
We need to implement `pwd` and `cd` in this part. Fist, We use to `strncmp()` to compare the first 2 or 3 chars to find out which operation we are doing, and then do operation.
#### `pwd`
For the `pwd`, we need to know what the current directory is. We can use `getcwd()` to finish it. It will return a current path for us and we just print this path on stdout, which is the terminal screen in this case.
#### `cd`
`cd` is a little bit complicated. We have to situations under `cd`. The first one is changing to the normal path. We can use `chdir` to do this, it will accept a string and change the path if any. Whenever we get the path after parsing, we could change directory. The second situation is `cd ..`, the strategy is same, get path and change to it. We use a loop to find where is the last `/` and delete all chars after that. Now, we get the new path and also use `chdir` to change directory.

### Output Redirection
After Parsing, we can divide this kind of command to 3 parts, a command, a `>` and a file. Then, We just use the same method that we used for the simple commands. Then, we need to create a unexist file using `open`. We use a several flags here to make sure open the file and we can write into it. For the `O_TRUNC`, that means if the regular file have already existed and we have permission to write, the file will be truncated to length 0. Next, before executing the command, we should figure out how we can write into the file. The answer is file descriptor. We use `dup2()` to close 2, which is `STDIN_FILENO`, and make 2 to that file, then we can write into the file. There is a point I want to mention is for our code. That is we must restore `STDIN_FILENO` to 2 to make sure we can still write on the terminal after output redirection. Therefore, we have a temporary storage to store the `STDIN_FILENO`, just use `dup(1)` to duplicate a `STDIN_FILENO` to do that.

### Pipeline commands


### Extra features
#### Standard error redirection
For `>&`, it is easy to do, we just keep the file connected with number 2 (`STDIN_FILENO`) until our program prints out the error message.

#### Directory stack
Here we use linked list to implement directory stack, because it is dynamic memory and also easy to insert or delete element.
##### `pushd`
Cause we just have two commands for the `pushd` and after parsing it we can get the command and the path we want to push. Then we can use `strcat()` to stick the current path, '/' and the pushed path. We have a whole path now and use `chdir()` to implement the `pushd`. Finally, we use a function to add the new path into the linked list.
##### `popd`

##### `dirs`
When the first time we execute the `dirs`, we need to get the current directory and add this new path into the directory stack. After, we initialize the directory stack 