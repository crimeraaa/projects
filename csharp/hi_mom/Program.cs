// Make sure CWD is this project's directory!
// 
// PROJECT CREATION
//      `dotnet new console --use-program-main`
// 
// BUILD AND RUN
//      `dotnet run`
namespace hi_mom;

class Program {
    static void Main(string[] args) {
        // Similar to C++ containers, `args` has a bunch of helpful methods!
        for (int i = 0; i < args.Count(); i++) {
            // Like C's printf, doesn't append a newline.
            // Like Lua's and Python's `print`, accepts different types.
            Console.Write(args.ElementAt(i) + ", ");
        }
        // Unlike C's printf, this appends a newline.
        Console.WriteLine("Hi mom!");
    }
}
