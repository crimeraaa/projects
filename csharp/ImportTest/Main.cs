using System;

namespace ImportTest;

class Program {
    static void Main(string[] args) {
        Console.WriteLine("Hi mom!");
        Module.Test.Write();
        string name = Module.Test.GetString("Enter your name: ");
        Console.WriteLine(string.Format("Hi {0}!", name));
    }
}
