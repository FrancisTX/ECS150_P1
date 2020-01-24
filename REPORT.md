## ECS150 Program Report                  
### Running the Simple commands
In this step, we implement the simple commands using `fork()`, `wait()` and `exec()`. Before we execute the commands, we will use `fork()` to produce a child process and execute the command in the child process. Until the child process is done, the parent process will be executed because we use `wait()` for waiting child. Then in the parent process, we use `execvp()` to execute the command we want. `execvp()` has two parameters, the `$PATH` and arguments array one. It will search this path and then execute the arguments after that. That is the things we want. 


### Arguments
Next, we need to figure out how to handle the arguments. We have to parsing the command line. Our paring strategy is list below. Apply an array of strings to store the parsed commands.
+ Scan the whole command with an infinite loop.
+ Append each char scanned into the end of the current word.
+ When hit a whitespace or `|` or `|&` or `>` or `.&`, update the number of `|` and `>` in the command line and store the current word into the array of strings. Then, store the symbol after the word into the array.
+ Continue until reaching the end of command.

### Built-in Command
We need to implement `pwd` and `cd` in this part. Fist, We use to `strncmp()` to compare the first 2 or 3 chars to find out which operation we are doing, and then do operation.
#### `pwd`
For the `pwd`, we need to know what the current directory is. We can use `getcwd()` to finish it. It will return a current path for us and we just print this path on stdout, which is the terminal screen in this case.
#### `cd`
`cd` is a little bit complicated. We have to situations under `cd`. The first one is changing to the normal path. We can use `chdir` to do this, it will accept a string and change the path if any. Whenever we get the path after parsing, we could change directory. The second situation is `cd ..`, the strategy is same, get path and change to it. We use a loop to find where is the last `/` and delete all chars after that. Now, we get the new path and also use `chdir` to change directory.

### Output Redirection
After Parsing, we can divide this kind of command to 3 parts, a command, a `>` and a file. Then, We just use the same method that we used for the simple commands. Then, we need to create a unexist file using `open`. We use a several flags here to make sure open the file and we can write into it. For the `O_TRUNC`, that means if the regular file have already existed and we have permission to write, the file will be truncated to length 0. Next, before executing the command, we should figure out how we can write into the file. The answer is file descriptor. We use `dup2()` to close 2, which is `STDIN_FILENO`, and make 2 to that file, then we can write into the file. There is a point I want to mention is for our code. That is we must restore `STDIN_FILENO` to 2 to make sure we can still write on the terminal after output redirection. Therefore, we have a temporary storage to store the `STDIN_FILENO`, just use `dup(1)` to duplicate a `STDIN_FILENO` to do that.

### Pipeline commands
In the process of parsing, we already count the number of `|` in the command line. If our count is larger than zero. Piping needs to be performed. First, we need to separate the parsed command line with multiple commands into a `struct` of that store each command individually. We do this by scanning the array of parsed commands. When we reach `|` or `|&`, the current command is complete and we need to store the next command. Then we can do piping. Before we pipe the last command, output of each command is connected to the input of the next command. We construct this with `fork()` and `dup2()` functions. We implement a helper function to connect file descriptors, whose parameters include the input and output of the command that we need to connect to in the file descriptor. In the child of the helper function, close `STDIN_FILENO` and open the input in parameters. The input of first command should remain `STDIN_FILENO`. Close `STDOUT_FILENO` and open the output in parameters. When implementing `|&`, connect `STDERR_FILENO` to the output instead. Then, we execute the command. The last command is done outside the helper function. We call `fork()` and in the child, close `STDIN_FILENO` and open output of the previous command before executing it.


### Extra features
#### Standard error redirection
For `>&` and `|&`, it is easy to do, we just keep the file connected with number 2 (`STDIN_FILENO`) until our program prints out the error message, use `dup2()`.

#### Directory stack
Here we use linked list to implement directory stack, because it is dynamic memory and also easy to insert or delete element. 
##### `dirs`
Let's talk about the directory stack first, because it is out thinking process and a proper date structure would be more easier to operate. Basically, when the first time we execute the `dirs`, we need to get the current directory and add this new path into the directory stack. After, we initialize the directory stack, we need to track the path whenever we use `pushd`,`popd`. So, we need to add or remove element in those two operation.The linked list has 3 variables, including the current directory, the count of the linked list and the next node. The head is the very first elements, and whenever we add (to the first here) an elements, head's next will be changes and if we want to remove value (from the last here), we can do some operation and set specific node's directory is null(explained below in `popd`).
##### `pushd`
Cause we just have two commands for the `pushd` and after parsing it we can get the command and the path we want to push. Then we can use `strcat()` to stick the current path, '/' and the pushed path. We have a whole path now and use `chdir()` to implement the `pushd`. Finally, we use a function to add the new path into the linked list.
##### `popd`
It is sounds like easy, because we just delete the very top element and change the directory to the second one in the stack. But it is a little bit hard for my single linked list to go very top elements. We must traversal almost all the node after the node and find the last one and set its directory to `NULL`. Here, we finish the first step and for the changing directory, just use chdir() to head's dierectory.